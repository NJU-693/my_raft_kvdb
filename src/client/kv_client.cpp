#include "client/kv_client.h"
#include <grpcpp/grpcpp.h>
#include "proto/raft.grpc.pb.h"
#include <iostream>

namespace client {

KVClient::KVClient(const std::vector<std::string>& server_addresses)
    : server_addresses_(server_addresses),
      timeout_ms_(5000),
      max_retries_(3) {
}

KVClient::~KVClient() {
    disconnect();
}

bool KVClient::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (server_addresses_.empty()) {
        last_error_ = "No server addresses provided";
        return false;
    }
    
    // 尝试查找 Leader
    current_leader_ = findLeader();
    
    return !current_leader_.empty();
}

void KVClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_leader_.clear();
}

bool KVClient::put(const std::string& key, const std::string& value) {
    std::string result;
    return executeOperation("PUT", key, value, result);
}

bool KVClient::get(const std::string& key, std::string& value) {
    return executeOperation("GET", key, "", value);
}

bool KVClient::remove(const std::string& key) {
    std::string result;
    return executeOperation("DELETE", key, "", result);
}

void KVClient::setTimeout(int timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    timeout_ms_ = timeout_ms;
}

void KVClient::setMaxRetries(int max_retries) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_retries_ = max_retries;
}

std::string KVClient::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

bool KVClient::sendRequest(
    const std::string& server_address,
    const std::string& operation,
    const std::string& key,
    const std::string& value,
    std::string& result) {
    
    // 创建 gRPC 通道
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    auto stub = raft::ClientService::NewStub(channel);
    
    // 准备请求
    raft::ClientRequest request;
    if (operation == "PUT") {
        request.set_operation(raft::ClientRequest::PUT);
        request.set_key(key);
        request.set_value(value);
    } else if (operation == "GET") {
        request.set_operation(raft::ClientRequest::GET);
        request.set_key(key);
    } else if (operation == "DELETE") {
        request.set_operation(raft::ClientRequest::DELETE);
        request.set_key(key);
    } else {
        last_error_ = "Unknown operation: " + operation;
        return false;
    }
    
    // 发送请求
    raft::ClientResponse response;
    grpc::ClientContext context;
    
    // 设置超时
    std::chrono::system_clock::time_point deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(timeout_ms_);
    context.set_deadline(deadline);
    
    grpc::Status status = stub->HandleRequest(&context, request, &response);
    
    if (!status.ok()) {
        last_error_ = "RPC failed: " + status.error_message();
        return false;
    }
    
    // 检查响应
    if (!response.is_leader()) {
        // 不是 Leader，更新 Leader 地址
        if (!response.leader_address().empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            current_leader_ = response.leader_address();
        }
        last_error_ = "Not leader, redirect to: " + response.leader_address();
        return false;
    }
    
    if (!response.success()) {
        last_error_ = response.error();
        return false;
    }
    
    // 成功
    if (operation == "GET") {
        result = response.value();
    }
    
    return true;
}

bool KVClient::executeOperation(
    const std::string& operation,
    const std::string& key,
    const std::string& value,
    std::string& result) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果没有已知的 Leader，先查找
    if (current_leader_.empty()) {
        current_leader_ = findLeader();
        if (current_leader_.empty()) {
            last_error_ = "Cannot find leader";
            return false;
        }
    }
    
    // 尝试发送请求（带重试）
    for (int retry = 0; retry < max_retries_; ++retry) {
        // 先尝试当前已知的 Leader
        if (sendRequest(current_leader_, operation, key, value, result)) {
            return true;
        }
        
        // 如果失败，尝试查找新的 Leader
        std::string new_leader = findLeader();
        if (new_leader.empty()) {
            last_error_ = "Cannot find leader after retry " + std::to_string(retry + 1);
            continue;
        }
        
        current_leader_ = new_leader;
        
        // 使用新 Leader 重试
        if (sendRequest(current_leader_, operation, key, value, result)) {
            return true;
        }
    }
    
    last_error_ = "Max retries exceeded";
    return false;
}

std::string KVClient::findLeader() {
    // 注意：此方法假设调用者已持有锁
    
    // std::cout << "[DEBUG] findLeader: Searching for leader among " << server_addresses_.size() << " servers" << std::endl;
    
    // 遍历所有服务器，查找 Leader
    for (const auto& address : server_addresses_) {
        // std::cout << "[DEBUG] findLeader: Trying " << address << std::endl;
        
        // 尝试发送一个简单的 GET 请求来探测 Leader
        std::string dummy_result;
        
        // 创建 gRPC 通道
        auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        auto stub = raft::ClientService::NewStub(channel);
        
        // 准备请求（使用一个不存在的 key 进行探测）
        raft::ClientRequest request;
        request.set_operation(raft::ClientRequest::GET);
        request.set_key("__probe__");
        
        // 发送请求
        raft::ClientResponse response;
        grpc::ClientContext context;
        
        // 设置较短的超时
        std::chrono::system_clock::time_point deadline =
            std::chrono::system_clock::now() + std::chrono::milliseconds(1000);
        context.set_deadline(deadline);
        
        grpc::Status status = stub->HandleRequest(&context, request, &response);
        
        // std::cout << "[DEBUG] findLeader: RPC status: " << (status.ok() ? "OK" : "FAILED") << std::endl;
        if (!status.ok()) {
            // std::cout << "[DEBUG] findLeader: Error: " << status.error_message() << std::endl;
        }
        
        if (status.ok()) {
            // std::cout << "[DEBUG] findLeader: is_leader=" << response.is_leader() << ", leader_address=" << response.leader_address() << std::endl;
            
            if (response.is_leader()) {
                // 找到 Leader
                // std::cout << "[DEBUG] findLeader: Found leader at " << address << std::endl;
                return address;
            } else if (!response.leader_address().empty()) {
                // 节点告诉我们 Leader 地址
                // std::cout << "[DEBUG] findLeader: Redirected to " << response.leader_address() << std::endl;
                return response.leader_address();
            }
        }
    }
    
    // std::cout << "[DEBUG] findLeader: No leader found" << std::endl;
    return "";  // 未找到 Leader
}

} // namespace client
