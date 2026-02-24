#ifndef NETWORK_SIMPLE_RPC_SERVER_H
#define NETWORK_SIMPLE_RPC_SERVER_H

#include "rpc_server.h"
#include <thread>
#include <atomic>
#include <mutex>

namespace network {

/**
 * @brief 简单的 RPC 服务器实现
 * 
 * 使用原生 socket 实现的轻量级 RPC 服务器。
 * 支持基本的请求-响应模式和超时机制。
 * 需求: 2.3.1 (节点间 RPC 通信)
 */
class SimpleRPCServer : public RaftRPCServer {
public:
    explicit SimpleRPCServer(raft::RaftNode* node);
    ~SimpleRPCServer() override;

    bool start(const std::string& address) override;
    void stop() override;
    void wait() override;
    void setRaftNode(raft::RaftNode* node) override;

private:
    void acceptLoop();
    void handleClient(int client_socket);
    std::string processRequest(const std::string& request);

    raft::RaftNode* raft_node_;
    std::atomic<bool> running_;
    std::thread accept_thread_;
    int server_socket_;
    std::string address_;
    std::mutex mutex_;
};

} // namespace network

#endif // NETWORK_SIMPLE_RPC_SERVER_H
