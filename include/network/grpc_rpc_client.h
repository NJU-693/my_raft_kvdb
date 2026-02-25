#ifndef NETWORK_GRPC_RPC_CLIENT_H
#define NETWORK_GRPC_RPC_CLIENT_H

#include "network/rpc_client.h"
#include <grpcpp/grpcpp.h>
#include "proto/raft.grpc.pb.h"
#include <memory>
#include <string>

namespace network {

/**
 * @brief gRPC 实现的 RPC 客户端（针对节点间通信，raft节点主动发起调用的都为客户端）
 * 
 * 继承 RaftRPCClient 接口，使用 gRPC 进行通信
 * 需求: 2.3.1 (节点间 RPC 通信)
 */
class GrpcRPCClient : public RaftRPCClient {
public:
    explicit GrpcRPCClient(const std::string& address);
    ~GrpcRPCClient() override;

    // RaftRPCClient 接口实现
    bool connect(const std::string& address) override;
    void disconnect() override;

    // 发送 RequestVote RPC（candidate竞选时）
    bool sendRequestVote(
        const ::raft::RequestVoteArgs& args,
        ::raft::RequestVoteReply& reply,
        int timeout_ms = 1000) override;

    // 发送 AppendEntries RPC（作为leader时）
    bool sendAppendEntries(
        const ::raft::AppendEntriesArgs& args,
        ::raft::AppendEntriesReply& reply,
        int timeout_ms = 1000) override;

    void setDefaultTimeout(int timeout_ms) override;
    void setRetries(int retries) override;

private:
    std::string server_address_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<raft::RaftService::Stub> stub_;
    int default_timeout_ms_;
    int max_retries_;

    // 辅助方法：C++ 对象转换为 proto 消息
    void requestVoteArgsToProto(
        const ::raft::RequestVoteArgs& args,
        raft::RequestVoteRequest* proto);
    
    void protoToRequestVoteReply(
        const raft::RequestVoteResponse* proto,
        ::raft::RequestVoteReply& reply);

    void appendEntriesArgsToProto(
        const ::raft::AppendEntriesArgs& args,
        raft::AppendEntriesRequest* proto);
    
    void protoToAppendEntriesReply(
        const raft::AppendEntriesResponse* proto,
        ::raft::AppendEntriesReply& reply);
};

} // namespace network

#endif // NETWORK_GRPC_RPC_CLIENT_H
