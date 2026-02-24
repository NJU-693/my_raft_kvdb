#ifndef NETWORK_RPC_SERVER_H
#define NETWORK_RPC_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include "raft/rpc_messages.h"

namespace raft {
    class RaftNode;
}

namespace network {

/**
 * @brief RPC 服务器接口
 * 
 * 提供 Raft RPC 服务的抽象接口，支持 gRPC 和其他实现。
 * 需求: 2.3.1 (节点间 RPC 通信)
 */
class RaftRPCServer {
public:
    virtual ~RaftRPCServer() = default;

    /**
     * @brief 启动 RPC 服务器
     * @param address 服务器地址（格式: "host:port"）
     * @return 是否成功启动
     */
    virtual bool start(const std::string& address) = 0;

    /**
     * @brief 停止 RPC 服务器
     */
    virtual void stop() = 0;

    /**
     * @brief 等待服务器关闭
     */
    virtual void wait() = 0;

    /**
     * @brief 设置 RaftNode 实例
     * @param node RaftNode 指针
     */
    virtual void setRaftNode(raft::RaftNode* node) = 0;
};

/**
 * @brief 创建 RPC 服务器实例
 * @param node RaftNode 指针
 * @return RPC 服务器实例
 */
std::unique_ptr<RaftRPCServer> createRPCServer(raft::RaftNode* node);

} // namespace network

#endif // NETWORK_RPC_SERVER_H
