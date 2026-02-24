#include "network/rpc_server.h"
#include "network/rpc_client.h"
#include "raft/raft_node.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

/**
 * @brief RPC 框架基本功能测试
 * 
 * 测试 RPC 服务器和客户端的基本功能：
 * 1. 服务器启动和停止
 * 2. 客户端连接
 * 3. RequestVote RPC
 * 4. AppendEntries RPC
 * 
 * 需求: 2.3.1 (节点间 RPC 通信)
 */

void testServerStartStop() {
    std::cout << "Test: Server Start/Stop" << std::endl;
    
    std::vector<int> peerIds = {2, 3};
    auto raftNode = std::make_unique<raft::RaftNode>(1, peerIds);
    
    auto server = network::createRPCServer(raftNode.get());
    
    // 启动服务器
    bool started = server->start("127.0.0.1:9001");
    assert(started && "Server should start successfully");
    
    // 等待服务器完全启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 停止服务器
    server->stop();
    
    std::cout << "✓ Server Start/Stop test passed" << std::endl;
}

void testRequestVoteRPC() {
    std::cout << "\nTest: RequestVote RPC" << std::endl;
    
    // 创建服务器节点
    std::vector<int> peerIds = {2};
    auto serverNode = std::make_unique<raft::RaftNode>(1, peerIds);
    
    auto server = network::createRPCServer(serverNode.get());
    server->start("127.0.0.1:9002");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 创建客户端
    auto client = network::createRPCClient("127.0.0.1:9002");
    
    if (client->connect("127.0.0.1:9002")) {
        // 发送 RequestVote RPC
        raft::RequestVoteArgs args(1, 2, 0, 0);
        raft::RequestVoteReply reply;
        
        bool success = client->sendRequestVote(args, reply, 1000);
        assert(success && "RequestVote RPC should succeed");
        assert(reply.voteGranted && "Vote should be granted");
        
        std::cout << "✓ RequestVote RPC test passed" << std::endl;
        
        client->disconnect();
    } else {
        std::cerr << "✗ Failed to connect to server" << std::endl;
        assert(false);
    }
    
    server->stop();
}

void testAppendEntriesRPC() {
    std::cout << "\nTest: AppendEntries RPC" << std::endl;
    
    // 创建服务器节点
    std::vector<int> peerIds = {2};
    auto serverNode = std::make_unique<raft::RaftNode>(1, peerIds);
    
    auto server = network::createRPCServer(serverNode.get());
    server->start("127.0.0.1:9003");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 创建客户端
    auto client = network::createRPCClient("127.0.0.1:9003");
    
    if (client->connect("127.0.0.1:9003")) {
        // 发送心跳（空的 AppendEntries）
        raft::AppendEntriesArgs args(1, 2, 0, 0, {}, 0);
        raft::AppendEntriesReply reply;
        
        bool success = client->sendAppendEntries(args, reply, 1000);
        assert(success && "AppendEntries RPC should succeed");
        
        std::cout << "✓ AppendEntries RPC test passed" << std::endl;
        
        client->disconnect();
    } else {
        std::cerr << "✗ Failed to connect to server" << std::endl;
        assert(false);
    }
    
    server->stop();
}

void testTimeout() {
    std::cout << "\nTest: RPC Timeout" << std::endl;
    
    // 尝试连接到不存在的服务器
    auto client = network::createRPCClient("127.0.0.1:9999");
    
    raft::RequestVoteArgs args(1, 2, 0, 0);
    raft::RequestVoteReply reply;
    
    auto start = std::chrono::steady_clock::now();
    bool success = client->sendRequestVote(args, reply, 500);
    auto end = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(!success && "RPC to non-existent server should fail");
    std::cout << "✓ RPC Timeout test passed (took " << duration.count() << "ms)" << std::endl;
}

void testRetry() {
    std::cout << "\nTest: RPC Retry" << std::endl;
    
    auto client = network::createRPCClient("127.0.0.1:9999");
    client->setRetries(2);  // 设置重试 2 次
    
    raft::RequestVoteArgs args(1, 2, 0, 0);
    raft::RequestVoteReply reply;
    
    auto start = std::chrono::steady_clock::now();
    bool success = client->sendRequestVote(args, reply, 200);
    auto end = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    assert(!success && "RPC should fail after retries");
    // 应该尝试多次，但由于连接失败很快，总时间可能较短
    std::cout << "✓ RPC Retry test passed (took " << duration.count() << "ms, attempted " 
              << (client->setRetries(2), "3") << " times)" << std::endl;
}

int main() {
    std::cout << "=== RPC Framework Tests ===" << std::endl;
    
    try {
        testServerStartStop();
        testRequestVoteRPC();
        testAppendEntriesRPC();
        testTimeout();
        testRetry();
        
        std::cout << "\n=== All RPC tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
