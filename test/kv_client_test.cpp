#include "client/kv_client.h"
#include "raft/raft_node.h"
#include "storage/kv_store.h"
#include "network/grpc_rpc_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <cassert>

/**
 * @brief KVClient 测试
 * 
 * 测试客户端接口的功能，包括：
 * - 客户端请求处理
 * - Leader 重定向
 * - 基本 KV 操作
 * 
 * 需求: 2.3.2 (客户端-服务器通信)
 */

// 测试辅助函数
void assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        exit(1);
    }
    std::cout << "PASSED: " << message << std::endl;
}

// 全局变量
std::unique_ptr<raft::RaftNode> node1;
std::unique_ptr<raft_kv::KVStore> kv_store1;
std::unique_ptr<network::GrpcRPCServer> server1;

void setup() {
    std::cout << "Setting up test environment..." << std::endl;
    
    // 创建单节点集群（节点1是唯一的节点）
    node1 = std::make_unique<raft::RaftNode>(1, std::vector<int>{});
    
    // 设置节点地址
    node1->setNodeAddress("localhost:50051");
    
    // 设置对等节点地址（空，因为是单节点）
    std::map<int, std::string> peer_addresses;
    peer_addresses[1] = "localhost:50051";
    node1->setPeerAddresses(peer_addresses);
    
    // 创建 KV 存储
    kv_store1 = std::make_unique<raft_kv::KVStore>();
    
    // 设置状态机应用回调
    node1->setApplyCallback([](const std::string& cmd) {
        return kv_store1->applyCommandWithResult(cmd);
    });
    
    // 创建 RPC 服务器
    server1 = std::make_unique<network::GrpcRPCServer>(node1.get());
    
    // 启动服务器
    assert(server1->start("0.0.0.0:50051"));
    
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 让节点1成为 Leader（单节点集群自动成为 Leader）
    node1->startElection();
    // 在单节点集群中，节点已经有自己的票，这是多数票
    // 需要手动调用 becomeLeader 或者检查是否已经是 Leader
    if (!node1->isLeader()) {
        // 单节点集群应该立即成为 Leader
        node1->becomeLeader();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    std::cout << "Test environment ready" << std::endl;
}

void teardown() {
    std::cout << "Tearing down test environment..." << std::endl;
    
    // 停止服务器
    if (server1) server1->stop();
    
    // 等待服务器停止
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "Test environment cleaned up" << std::endl;
}

// 辅助方法：确保节点1是 Leader
void ensureNode1IsLeader() {
    if (!node1->isLeader()) {
        node1->startElection();
        node1->becomeLeader();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    assert(node1->isLeader());
}

/**
 * @brief 测试客户端连接
 */
void testClientConnect() {
    std::cout << "\n=== Test: Client Connect ===" << std::endl;
    
    // 先检查节点状态
    std::cout << "Node1 is leader: " << node1->isLeader() << std::endl;
    std::cout << "Node1 leader address: " << node1->getLeaderAddress() << std::endl;
    
    std::vector<std::string> addresses = {
        "localhost:50051"
    };
    
    client::KVClient client(addresses);
    
    // 连接应该成功，因为节点1是 Leader
    bool connected = client.connect();
    if (!connected) {
        std::cout << "Connection failed. Last error: " << client.getLastError() << std::endl;
    }
    assertTrue(connected, "Client should connect successfully");
}

/**
 * @brief 测试基本 Put 操作
 */
void testBasicPutOperation() {
    std::cout << "\n=== Test: Basic Put Operation ===" << std::endl;
    
    // 确保节点1是 Leader
    ensureNode1IsLeader();
    
    std::vector<std::string> addresses = {"localhost:50051"};
    client::KVClient client(addresses);
    
    // 执行 Put 操作
    bool success = client.put("test_key", "test_value");
    assertTrue(success, "Put operation should succeed");
}

/**
 * @brief 测试基本 Get 操作
 */
void testBasicGetOperation() {
    std::cout << "\n=== Test: Basic Get Operation ===" << std::endl;
    
    // 确保节点1是 Leader
    ensureNode1IsLeader();
    
    std::vector<std::string> addresses = {"localhost:50051"};
    client::KVClient client(addresses);
    
    // 先 Put 一个值
    bool put_success = client.put("get_key", "get_value");
    assertTrue(put_success, "Put operation should succeed");
    
    // 等待命令被应用
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 执行 Get 操作
    std::string value;
    bool get_success = client.get("get_key", value);
    assertTrue(get_success, "Get operation should succeed");
    assertTrue(value == "get_value", "Retrieved value should match");
}

/**
 * @brief 测试 Get 不存在的键
 */
void testGetNonExistentKey() {
    std::cout << "\n=== Test: Get Non-Existent Key ===" << std::endl;
    
    // 确保节点1是 Leader
    ensureNode1IsLeader();
    
    std::vector<std::string> addresses = {"localhost:50051"};
    client::KVClient client(addresses);
    
    // 尝试 Get 不存在的键
    std::string value;
    bool success = client.get("non_existent_key", value);
    assertTrue(!success, "Get non-existent key should fail");
}

/**
 * @brief 测试基本 Delete 操作
 */
void testBasicDeleteOperation() {
    std::cout << "\n=== Test: Basic Delete Operation ===" << std::endl;
    
    // 确保节点1是 Leader
    ensureNode1IsLeader();
    
    std::vector<std::string> addresses = {"localhost:50051"};
    client::KVClient client(addresses);
    
    // 先 Put 一个值
    bool put_success = client.put("delete_key", "delete_value");
    assertTrue(put_success, "Put operation should succeed");
    
    // 等待命令被应用
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 执行 Delete 操作
    bool delete_success = client.remove("delete_key");
    assertTrue(delete_success, "Delete operation should succeed");
    
    // 等待命令被应用
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 验证键已被删除
    std::string value;
    bool get_success = client.get("delete_key", value);
    assertTrue(!get_success, "Get deleted key should fail");
}

/**
 * @brief 测试超时和重试设置
 */
void testTimeoutAndRetrySetting() {
    std::cout << "\n=== Test: Timeout and Retry Setting ===" << std::endl;
    
    std::vector<std::string> addresses = {"localhost:50051"};
    client::KVClient client(addresses);
    
    // 设置超时时间
    client.setTimeout(1000);
    
    // 设置最大重试次数
    client.setMaxRetries(5);
    
    std::cout << "Timeout and retry settings applied successfully" << std::endl;
}

int main() {
    std::cout << "Starting KV Client Tests..." << std::endl;
    
    try {
        setup();
        
        testClientConnect();
        testBasicPutOperation();
        testBasicGetOperation();
        testGetNonExistentKey();
        testBasicDeleteOperation();
        testTimeoutAndRetrySetting();
        
        teardown();
        
        std::cout << "\n=== All tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        teardown();
        return 1;
    }
}
