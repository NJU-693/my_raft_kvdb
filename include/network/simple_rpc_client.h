#ifndef NETWORK_SIMPLE_RPC_CLIENT_H
#define NETWORK_SIMPLE_RPC_CLIENT_H

#include "rpc_client.h"
#include <mutex>

namespace network {

/**
 * @brief 简单的 RPC 客户端实现
 * 
 * 使用原生 socket 实现的轻量级 RPC 客户端。
 * 支持超时和重试机制。
 * 需求: 2.3.1 (节点间 RPC 通信)
 */
class SimpleRPCClient : public RaftRPCClient {
public:
    explicit SimpleRPCClient(const std::string& address);
    ~SimpleRPCClient() override;

    bool connect(const std::string& address) override;
    void disconnect() override;

    bool sendRequestVote(
        const raft::RequestVoteArgs& args,
        raft::RequestVoteReply& reply,
        int timeout_ms = 1000) override;

    bool sendAppendEntries(
        const raft::AppendEntriesArgs& args,
        raft::AppendEntriesReply& reply,
        int timeout_ms = 1000) override;

    void setDefaultTimeout(int timeout_ms) override;
    void setRetries(int retries) override;

private:
    bool sendRequest(const std::string& request, std::string& response, int timeout_ms);
    bool connectWithTimeout(int timeout_ms);

    std::string address_;
    int socket_;
    int default_timeout_ms_;
    int retries_;
    std::mutex mutex_;
    bool connected_;
};

} // namespace network

#endif // NETWORK_SIMPLE_RPC_CLIENT_H
