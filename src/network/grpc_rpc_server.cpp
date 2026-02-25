#include "network/grpc_rpc_server.h"
#include <iostream>

namespace network {

// ==================== 辅助函数 ====================

static void protoToRequestVoteArgs(
    const raft::RequestVoteRequest* proto,
    ::raft::RequestVoteArgs& args) {
    
    args.term = proto->term();
    args.candidateId = proto->candidate_id();
    args.lastLogIndex = proto->last_log_index();
    args.lastLogTerm = proto->last_log_term();
}

static void requestVoteReplyToProto(
    const ::raft::RequestVoteReply& reply,
    raft::RequestVoteResponse* proto) {
    
    proto->set_term(reply.term);
    proto->set_vote_granted(reply.voteGranted);
}

static void protoToAppendEntriesArgs(
    const raft::AppendEntriesRequest* proto,
    ::raft::AppendEntriesArgs& args) {
    
    args.term = proto->term();
    args.leaderId = proto->leader_id();
    args.prevLogIndex = proto->prev_log_index();
    args.prevLogTerm = proto->prev_log_term();
    args.leaderCommit = proto->leader_commit();
    
    // 转换日志条目
    args.entries.clear();
    for (const auto& entry_proto : proto->entries()) {
        ::raft::LogEntry entry;
        entry.term = entry_proto.term();
        entry.index = entry_proto.index();
        entry.command = entry_proto.command();
        args.entries.push_back(entry);
    }
}

static void appendEntriesReplyToProto(
    const ::raft::AppendEntriesReply& reply,
    raft::AppendEntriesResponse* proto) {
    
    proto->set_term(reply.term);
    proto->set_success(reply.success);
    proto->set_conflict_index(reply.conflictIndex);
    proto->set_conflict_term(reply.conflictTerm);
}

// ==================== RaftServiceImpl ====================

grpc::Status RaftServiceImpl::RequestVote(
    grpc::ServerContext* /*context*/,
    const raft::RequestVoteRequest* request,
    raft::RequestVoteResponse* response) {
    
    if (!raft_node_) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "RaftNode not set");
    }
    
    // 转换 proto 消息为 C++ 对象
    ::raft::RequestVoteArgs args;
    protoToRequestVoteArgs(request, args);
    
    // 调用 RaftNode 处理
    ::raft::RequestVoteReply reply;
    raft_node_->handleRequestVote(args, reply);
    
    // 转换 C++ 对象为 proto 消息
    requestVoteReplyToProto(reply, response);
    
    return grpc::Status::OK;
}

grpc::Status RaftServiceImpl::AppendEntries(
    grpc::ServerContext* /*context*/,
    const raft::AppendEntriesRequest* request,
    raft::AppendEntriesResponse* response) {
    
    if (!raft_node_) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "RaftNode not set");
    }
    
    // 转换 proto 消息为 C++ 对象
    ::raft::AppendEntriesArgs args;
    protoToAppendEntriesArgs(request, args);
    
    // 调用 RaftNode 处理
    ::raft::AppendEntriesReply reply;
    raft_node_->handleAppendEntries(args, reply);
    
    // 转换 C++ 对象为 proto 消息
    appendEntriesReplyToProto(reply, response);
    
    return grpc::Status::OK;
}

// ==================== ClientServiceImpl ====================

grpc::Status ClientServiceImpl::HandleRequest(
    grpc::ServerContext* /*context*/,
    const raft::ClientRequest* request,
    raft::ClientResponse* response) {
    
    if (!raft_node_) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "RaftNode not set");
    }
    
    // 检查是否为 Leader
    if (!raft_node_->isLeader()) {
        response->set_success(false);
        response->set_is_leader(false);
        response->set_error("Not leader");
        
        // 提供 Leader 地址用于重定向
        std::string leader_address = raft_node_->getLeaderAddress();
        if (!leader_address.empty()) {
            response->set_leader_address(leader_address);
        }
        
        return grpc::Status::OK;
    }
    
    // 构造命令字符串
    std::string command;
    switch (request->operation()) {
        case raft::ClientRequest::PUT:
            command = "PUT:" + request->key() + ":" + request->value();
            break;
        case raft::ClientRequest::GET:
            command = "GET:" + request->key();
            break;
        case raft::ClientRequest::DELETE:
            command = "DELETE:" + request->key();
            break;
        default:
            response->set_success(false);
            response->set_is_leader(true);
            response->set_error("Unknown operation");
            return grpc::Status::OK;
    }
    
    // 提交命令到 Raft（增加超时时间到 10 秒）
    std::string result;
    bool success = raft_node_->submitCommand(command, result, 10000);
    
    response->set_success(success);
    response->set_is_leader(true);
    
    if (success) {
        // 对于 GET 操作，需要解析结果
        if (request->operation() == raft::ClientRequest::GET) {
            // 结果格式可能是 "VALUE:xxx" 或 "NOT_FOUND"
            if (result.find("VALUE:") == 0) {
                response->set_value(result.substr(6));  // 跳过 "VALUE:"
            } else if (result == "NOT_FOUND") {
                response->set_success(false);
                response->set_error("Key not found");
            } else {
                response->set_value(result);
            }
        }
    } else {
        response->set_error("Command execution failed or timeout");
    }
    
    return grpc::Status::OK;
}

// ==================== GrpcRPCServer ====================

GrpcRPCServer::GrpcRPCServer(raft::RaftNode* node)
    : raft_node_(node), 
      server_(nullptr),
      raft_service_(std::make_unique<RaftServiceImpl>(node)),
      client_service_(std::make_unique<ClientServiceImpl>(node)) {
}

GrpcRPCServer::~GrpcRPCServer() {
    stop();
}

bool GrpcRPCServer::start(const std::string& address) {
    server_address_ = address;
    
    grpc::ServerBuilder builder;
    
    // 配置服务器地址（不使用 SSL）
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    
    // 注册服务
    builder.RegisterService(raft_service_.get());
    builder.RegisterService(client_service_.get());
    
    // 构建并启动服务器
    server_ = builder.BuildAndStart();
    
    if (!server_) {
        std::cerr << "Failed to start gRPC server on " << address << std::endl;
        return false;
    }
    
    std::cout << "gRPC server listening on " << address << std::endl;
    return true;
}

void GrpcRPCServer::stop() {
    if (server_) {
        server_->Shutdown();
        server_.reset();
    }
}

void GrpcRPCServer::wait() {
    if (server_) {
        server_->Wait();
    }
}

void GrpcRPCServer::setRaftNode(raft::RaftNode* node) {
    raft_node_ = node;
    if (raft_service_) {
        raft_service_ = std::make_unique<RaftServiceImpl>(node);
    }
    if (client_service_) {
        client_service_ = std::make_unique<ClientServiceImpl>(node);
    }
}

} // namespace network
