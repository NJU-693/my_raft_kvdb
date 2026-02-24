#include "network/simple_rpc_client.h"
#include <iostream>
#include <cstring>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace network {

namespace {
    constexpr int BUFFER_SIZE = 65536;  // 64KB buffer

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

SimpleRPCClient::SimpleRPCClient(const std::string& address)
    : address_(address)
    , socket_(INVALID_SOCKET)
    , default_timeout_ms_(1000)
    , retries_(3)
    , connected_(false) {
    initSockets();
}

SimpleRPCClient::~SimpleRPCClient() {
    disconnect();
    cleanupSockets();
}

bool SimpleRPCClient::connect(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        disconnect();
    }

    address_ = address;
    return connectWithTimeout(default_timeout_ms_);
}

void SimpleRPCClient::disconnect() {
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
    connected_ = false;
}

bool SimpleRPCClient::sendRequestVote(
    const raft::RequestVoteArgs& args,
    raft::RequestVoteReply& reply,
    int timeout_ms) {
    
    std::string request = "RequestVote|" + args.serialize();
    std::string response;

    for (int attempt = 0; attempt <= retries_; ++attempt) {
        if (sendRequest(request, response, timeout_ms)) {
            try {
                // 响应格式: "OK|payload" 或 "ERROR:message"
                if (response.substr(0, 3) == "OK|") {
                    reply = raft::RequestVoteReply::deserialize(response.substr(3));
                    return true;
                } else {
                    std::cerr << "RequestVote error: " << response << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse RequestVote response: " << e.what() << std::endl;
            }
        }
        
        if (attempt < retries_) {
            disconnect();
        }
    }

    return false;
}

bool SimpleRPCClient::sendAppendEntries(
    const raft::AppendEntriesArgs& args,
    raft::AppendEntriesReply& reply,
    int timeout_ms) {
    
    std::string request = "AppendEntries|" + args.serialize();
    std::string response;

    for (int attempt = 0; attempt <= retries_; ++attempt) {
        if (sendRequest(request, response, timeout_ms)) {
            try {
                // 响应格式: "OK|payload" 或 "ERROR:message"
                if (response.substr(0, 3) == "OK|") {
                    reply = raft::AppendEntriesReply::deserialize(response.substr(3));
                    return true;
                } else {
                    std::cerr << "AppendEntries error: " << response << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse AppendEntries response: " << e.what() << std::endl;
            }
        }
        
        if (attempt < retries_) {
            disconnect();
        }
    }

    return false;
}

void SimpleRPCClient::setDefaultTimeout(int timeout_ms) {
    default_timeout_ms_ = timeout_ms;
}

void SimpleRPCClient::setRetries(int retries) {
    retries_ = retries;
}

bool SimpleRPCClient::sendRequest(const std::string& request, std::string& response, int timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 如果未连接，尝试连接
    if (!connected_) {
        if (!connectWithTimeout(timeout_ms)) {
            return false;
        }
    }

    // 发送请求
    int bytes_sent = send(socket_, request.c_str(), request.length(), 0);
    if (bytes_sent == SOCKET_ERROR) {
        std::cerr << "Failed to send request" << std::endl;
        connected_ = false;
        return false;
    }

    // 设置接收超时
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, 
               reinterpret_cast<const char*>(&tv), sizeof(tv));

    // 接收响应
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(socket_, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive response or timeout" << std::endl;
        connected_ = false;
        return false;
    }

    buffer[bytes_received] = '\0';
    response = std::string(buffer, bytes_received);
    
    return true;
}

bool SimpleRPCClient::connectWithTimeout(int timeout_ms) {
    // 解析地址和端口
    size_t colon_pos = address_.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Invalid address format: " << address_ << std::endl;
        return false;
    }

    std::string host = address_.substr(0, colon_pos);
    int port = std::stoi(address_.substr(colon_pos + 1));

    // 创建 socket
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置连接超时
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, 
               reinterpret_cast<const char*>(&tv), sizeof(tv));

    // 连接到服务器
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    if (::connect(socket_, reinterpret_cast<sockaddr*>(&server_addr), 
                  sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }

    connected_ = true;
    return true;
}

} // namespace network
