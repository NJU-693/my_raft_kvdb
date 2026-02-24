#include "network/grpc_rpc_client.h"
#include <iostream>
#include <chrono>

namespace network {

GrpcRPCClient::GrpcRPCClient(const std::string& address)
    : server_address_(address),
      default_timeout_ms_(1000),
      max_retries_(3) {
    connect(address);
}

GrpcRPCClient::~GrpcRPCClient() {
    disconnect();
}

bool GrpcRPCClient::connect(const std::string& address) {
    server_address_ = address;
    
    // 创建 gRPC 通道（不使用 SSL）
    channel_ = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    
    if (!channel_) {
        std::cerr << "Failed to create gRPC channel to " << address << std::endl;
        return false;
    }
    
    // 创建 stub
    stub_ = raft::RaftService::NewStub(channel_);
    
    if (!stub_) {
        std::cerr << "Failed to create gRPC stub for " << address << std::endl;
        return false;
    }
    
    return true;
}

void GrpcRPCClient::disconnect() {
    stub_.reset();
    channel_.reset();
}

bool GrpcRPCClient::sendRequestVote(
    const ::raft::RequestVoteArgs& args,
    ::raft::RequestVoteReply& reply,
    int timeout_ms) {
    
    if (!stub_) {
        std::cerr << "gRPC stub not initialized" << std::endl;
        return false;
    }
    
    // 转换 C++ 对象为 proto 消息
    raft::RequestVoteRequest request;
    requestVoteArgsToProto(args, &request);
    
    raft::RequestVoteResponse response;
    grpc::ClientContext context;
    
    // 设置超时
    auto deadline = std::chrono::system_clock::now() + 
                    std::chrono::milliseconds(timeout_ms);
    context.set_deadline(deadline);
    
    // 发送 RPC 请求
    grpc::Status status = stub_->RequestVote(&context, request, &response);
    
    if (!status.ok()) {
        std::cerr << "RequestVote RPC failed: " << status.error_message() << std::endl;
        return false;
    }
    
    // 转换 proto 消息为 C++ 对象
    protoToRequestVoteReply(&response, reply);
    
    return true;
}

bool GrpcRPCClient::sendAppendEntries(
    const ::raft::AppendEntriesArgs& args,
    ::raft::AppendEntriesReply& reply,
    int timeout_ms) {
    
    if (!stub_) {
        std::cerr << "gRPC stub not initialized" << std::endl;
        return false;
    }
    
    // 转换 C++ 对象为 proto 消息
    raft::AppendEntriesRequest request;
    appendEntriesArgsToProto(args, &request);
    
    raft::AppendEntriesResponse response;
    grpc::ClientContext context;
    
    // 设置超时
    auto deadline = std::chrono::system_clock::now() + 
                    std::chrono::milliseconds(timeout_ms);
    context.set_deadline(deadline);
    
    // 发送 RPC 请求
    grpc::Status status = stub_->AppendEntries(&context, request, &response);
    
    if (!status.ok()) {
        std::cerr << "AppendEntries RPC failed: " << status.error_message() << std::endl;
        return false;
    }
    
    // 转换 proto 消息为 C++ 对象
    protoToAppendEntriesReply(&response, reply);
    
    return true;
}

void GrpcRPCClient::setDefaultTimeout(int timeout_ms) {
    default_timeout_ms_ = timeout_ms;
}

void GrpcRPCClient::setRetries(int retries) {
    max_retries_ = retries;
}

// 辅助方法实现

void GrpcRPCClient::requestVoteArgsToProto(
    const ::raft::RequestVoteArgs& args,
    raft::RequestVoteRequest* proto) {
    
    proto->set_term(args.term);
    proto->set_candidate_id(args.candidateId);
    proto->set_last_log_index(args.lastLogIndex);
    proto->set_last_log_term(args.lastLogTerm);
}

void GrpcRPCClient::protoToRequestVoteReply(
    const raft::RequestVoteResponse* proto,
    ::raft::RequestVoteReply& reply) {
    
    reply.term = proto->term();
    reply.voteGranted = proto->vote_granted();
}

void GrpcRPCClient::appendEntriesArgsToProto(
    const ::raft::AppendEntriesArgs& args,
    raft::AppendEntriesRequest* proto) {
    
    proto->set_term(args.term);
    proto->set_leader_id(args.leaderId);
    proto->set_prev_log_index(args.prevLogIndex);
    proto->set_prev_log_term(args.prevLogTerm);
    proto->set_leader_commit(args.leaderCommit);
    
    // 转换日志条目
    for (const auto& entry : args.entries) {
        auto* entry_proto = proto->add_entries();
        entry_proto->set_term(entry.term);
        entry_proto->set_index(entry.index);
        entry_proto->set_command(entry.command);
    }
}

void GrpcRPCClient::protoToAppendEntriesReply(
    const raft::AppendEntriesResponse* proto,
    ::raft::AppendEntriesReply& reply) {
    
    reply.term = proto->term();
    reply.success = proto->success();
    reply.conflictIndex = proto->conflict_index();
    reply.conflictTerm = proto->conflict_term();
}

} // namespace network
