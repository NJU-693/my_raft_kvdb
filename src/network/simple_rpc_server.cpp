#include "network/simple_rpc_server.h"
#include "raft/raft_node.h"
#include <iostream>
#include <cstring>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace network {

namespace {
    constexpr int BUFFER_SIZE = 65536;  // 64KB buffer
    constexpr int BACKLOG = 10;

    void initSockets() {
#ifdef _WIN32
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    }

    void cleanupSockets() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

SimpleRPCServer::SimpleRPCServer(raft::RaftNode* node)
    : raft_node_(node)
    , running_(false)
    , server_socket_(INVALID_SOCKET) {
    initSockets();
}

SimpleRPCServer::~SimpleRPCServer() {
    stop();
    cleanupSockets();
}

bool SimpleRPCServer::start(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        return false;
    }

    // 解析地址和端口
    size_t colon_pos = address.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid address format: " << address << std::endl;
        return false;
    }

    std::string host = address.substr(0, colon_pos);
    int port = std::stoi(address.substr(colon_pos + 1));

    // 创建 socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置 socket 选项
    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // 绑定地址
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (host == "0.0.0.0" || host.empty()) {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);
    }

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&server_addr), 
             sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket to " << address << std::endl;
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
        return false;
    }

    // 监听
    if (listen(server_socket_, BACKLOG) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on socket" << std::endl;
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
        return false;
    }

    address_ = address;
    running_ = true;
    accept_thread_ = std::thread(&SimpleRPCServer::acceptLoop, this);

    std::cout << "RPC Server started on " << address << std::endl;
    return true;
}

void SimpleRPCServer::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }

    if (server_socket_ != INVALID_SOCKET) {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    std::cout << "RPC Server stopped" << std::endl;
}

void SimpleRPCServer::wait() {
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

void SimpleRPCServer::setRaftNode(raft::RaftNode* node) {
    std::lock_guard<std::mutex> lock(mutex_);
    raft_node_ = node;
}

void SimpleRPCServer::acceptLoop() {
    while (running_) {
        // 设置 accept 超时，以便能够检查 running_ 标志
        struct timeval tv;
        tv.tv_sec = 1;  // 1 秒超时
        tv.tv_usec = 0;
        setsockopt(server_socket_, SOL_SOCKET, SO_RCVTIMEO, 
                   reinterpret_cast<const char*>(&tv), sizeof(tv));

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket_, 
                                   reinterpret_cast<sockaddr*>(&client_addr), 
                                   &client_len);
        
        if (client_socket == INVALID_SOCKET) {
            // 超时或错误，继续循环检查 running_
            continue;
        }

        // 在新线程中处理客户端请求
        std::thread(&SimpleRPCServer::handleClient, this, client_socket).detach();
    }
}

void SimpleRPCServer::handleClient(int client_socket) {
    char buffer[BUFFER_SIZE];
    
    // 接收请求
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    buffer[bytes_received] = '\0';
    std::string request(buffer, bytes_received);

    // 处理请求
    std::string response = processRequest(request);

    // 发送响应
    send(client_socket, response.c_str(), response.length(), 0);
    
    closesocket(client_socket);
}

std::string SimpleRPCServer::processRequest(const std::string& request) {
    if (!raft_node_) {
        return "ERROR:RaftNode not set";
    }

    try {
        // 请求格式: "METHOD|payload"
        size_t delimiter_pos = request.find('|');
        if (delimiter_pos == std::string::npos) {
            return "ERROR:Invalid request format";
        }

        std::string method = request.substr(0, delimiter_pos);
        std::string payload = request.substr(delimiter_pos + 1);

        if (method == "RequestVote") {
            raft::RequestVoteArgs args = raft::RequestVoteArgs::deserialize(payload);
            raft::RequestVoteReply reply;
            raft_node_->handleRequestVote(args, reply);
            return "OK|" + reply.serialize();
        } else if (method == "AppendEntries") {
            raft::AppendEntriesArgs args = raft::AppendEntriesArgs::deserialize(payload);
            raft::AppendEntriesReply reply;
            raft_node_->handleAppendEntries(args, reply);
            return "OK|" + reply.serialize();
        } else {
            return "ERROR:Unknown method: " + method;
        }
    } catch (const std::exception& e) {
        return std::string("ERROR:") + e.what();
    }
}

} // namespace network
