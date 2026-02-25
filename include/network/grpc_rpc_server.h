#ifndef NETWORK_GRPC_RPC_SERVER_H
#define NETWORK_GRPC_RPC_SERVER_H

#include "network/rpc_server.h"
#include "raft/raft_node.h"
#include <grpcpp/grpcpp.h>
#include "proto/raft.grpc.pb.h"
#include <memory>
#include <string>

namespace network {

/**
 * @brief gRPC RaftService 实现（针对于系统内部节点间通信的服务）
 * raft节点间的通信（服务端角色）
 */
class RaftServiceImpl final : public raft::RaftService::Service {
public:
    explicit RaftServiceImpl(raft::RaftNode* node) : raft_node_(node) {}

    grpc::Status RequestVote(
        grpc::ServerContext* context,
        const raft::RequestVoteRequest* request,
        raft::RequestVoteResponse* response) override;

    grpc::Status AppendEntries(
        grpc::ServerContext* context,
        const raft::AppendEntriesRequest* request,
        raft::AppendEntriesResponse* response) override;

private:
    raft::RaftNode* raft_node_;
};

/**
 * @brief gRPC ClientService 实现（针对于整个系统对外客户端的服务）
 * 处理 外部 客户端的KV操作请求（服务端角色）
 */
class ClientServiceImpl final : public raft::ClientService::Service {
public:
    explicit ClientServiceImpl(raft::RaftNode* node) : raft_node_(node) {}

    grpc::Status HandleRequest(
        grpc::ServerContext* context,
        const raft::ClientRequest* request,
        raft::ClientResponse* response) override;

private:
    raft::RaftNode* raft_node_;
};

/**
 * @brief gRPC 实现的 RPC 服务器
 * 
 * 继承 RaftRPCServer 接口和 gRPC 生成的 RaftService::Service
 * 需求: 2.3.1 (节点间 RPC 通信), 2.3.2 (客户端-服务器通信)
 */
class GrpcRPCServer : public RaftRPCServer {
public:
    explicit GrpcRPCServer(raft::RaftNode* node);
    ~GrpcRPCServer() override;

    // RaftRPCServer 接口实现
    bool start(const std::string& address) override;
    void stop() override;
    void wait() override;
    void setRaftNode(raft::RaftNode* node) override;

private:
    raft::RaftNode* raft_node_;
    std::unique_ptr<grpc::Server> server_;
    std::string server_address_;
    
    // 服务实现
    std::unique_ptr<RaftServiceImpl> raft_service_;
    std::unique_ptr<ClientServiceImpl> client_service_;
};

} // namespace network

#endif // NETWORK_GRPC_RPC_SERVER_H
