#ifndef NETWORK_RPC_CLIENT_H
#define NETWORK_RPC_CLIENT_H

#include <string>
#include <memory>
#include <chrono>
#include "raft/rpc_messages.h"

namespace network {

/**
 * @brief RPC 客户端接口
 * 
 * 提供 Raft RPC 客户端的抽象接口，支持 gRPC 和其他实现。
 * 需求: 2.3.1 (节点间 RPC 通信)
 */
class RaftRPCClient {
public:
    virtual ~RaftRPCClient() = default;

    /**
     * @brief 连接到远程节点
     * @param address 远程节点地址（格式: "host:port"）
     * @return 是否成功连接
     */
    virtual bool connect(const std::string& address) = 0;

    /**
     * @brief 断开连接
     */
    virtual void disconnect() = 0;

    /**
     * @brief 发送 RequestVote RPC
     * @param args 请求参数
     * @param reply 响应结果
     * @param timeout 超时时间（毫秒）
     * @return 是否成功
     */
    virtual bool sendRequestVote(
        const raft::RequestVoteArgs& args,
        raft::RequestVoteReply& reply,
        int timeout_ms = 1000) = 0;

    /**
     * @brief 发送 AppendEntries RPC
     * @param args 请求参数
     * @param reply 响应结果
     * @param timeout 超时时间（毫秒）
     * @return 是否成功
     */
    virtual bool sendAppendEntries(
        const raft::AppendEntriesArgs& args,
        raft::AppendEntriesReply& reply,
        int timeout_ms = 1000) = 0;

    /**
     * @brief 设置默认超时时间
     * @param timeout_ms 超时时间（毫秒）
     */
    virtual void setDefaultTimeout(int timeout_ms) = 0;

    /**
     * @brief 设置重试次数
     * @param retries 重试次数
     */
    virtual void setRetries(int retries) = 0;
};

/**
 * @brief 创建 RPC 客户端实例
 * @param address 远程节点地址
 * @return RPC 客户端实例
 */
std::unique_ptr<RaftRPCClient> createRPCClient(const std::string& address);

} // namespace network

#endif // NETWORK_RPC_CLIENT_H
