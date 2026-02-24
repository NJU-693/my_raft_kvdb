#include "network/rpc_server.h"
#include "network/rpc_client.h"
#include "raft/raft_node.h"
#include <iostream>
#include <thread>
#include <chrono>

/**
 * @brief RPC 框架使用示例
 * 
 * 演示如何使用 RPC 框架进行节点间通信：
 * 1. 启动多个 RPC 服务器
 * 2. 使用客户端发送 RPC 请求
 * 3. 处理响应
 * 
 * 需求: 2.3.1 (节点间 RPC 通信)
 */

int main() {
    std::cout << "=== RPC Framework Demo ===" << std::endl;
    
    // 创建三个 Raft 节点
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    std::vector<int> peers3 = {1, 2};
    
    auto node1 = std::make_unique<raft::RaftNode>(1, peers1);
    auto node2 = std::make_unique<raft::RaftNode>(2, peers2);
    auto node3 = std::make_unique<raft::RaftNode>(3, peers3);
    
    // 创建并启动 RPC 服务器
    auto server1 = network::createRPCServer(node1.get());
    auto server2 = network::createRPCServer(node2.get());
    auto server3 = network::createRPCServer(node3.get());
    
    std::cout << "\n1. Starting RPC servers..." << std::endl;
    server1->start("127.0.0.1:8001");
    server2->start("127.0.0.1:8002");
    server3->start("127.0.0.1:8003");
    
    // 等待服务器完全启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "   ✓ All servers started" << std::endl;
    
    // 创建 RPC 客户端
    std::cout << "\n2. Creating RPC clients..." << std::endl;
    auto client_to_2 = network::createRPCClient("127.0.0.1:8002");
    auto client_to_3 = network::createRPCClient("127.0.0.1:8003");
    std::cout << "   ✓ Clients created" << std::endl;
    
    // 节点 1 向节点 2 和 3 发送 RequestVote
    std::cout << "\n3. Node 1 requesting votes from Node 2 and Node 3..." << std::endl;
    
    // 节点 1 发起选举
    node1->startElection();
    int term = node1->getCurrentTerm();
    int lastLogIndex = node1->getLastLogIndex();
    int lastLogTerm = node1->getLastLogTerm();
    
    // 向节点 2 请求投票
    if (client_to_2->connect("127.0.0.1:8002")) {
        raft::RequestVoteArgs args(term, 1, lastLogIndex, lastLogTerm);
        raft::RequestVoteReply reply;
        
        if (client_to_2->sendRequestVote(args, reply, 1000)) {
            std::cout << "   Node 2 response: term=" << reply.term 
                      << ", voteGranted=" << (reply.voteGranted ? "true" : "false") << std::endl;
            
            // 处理投票响应
            if (node1->handleVoteResponse(2, reply)) {
                std::cout << "   ✓ Node 1 became Leader!" << std::endl;
            }
        }
        client_to_2->disconnect();
    }
    
    // 向节点 3 请求投票
    if (client_to_3->connect("127.0.0.1:8003")) {
        raft::RequestVoteArgs args(term, 1, lastLogIndex, lastLogTerm);
        raft::RequestVoteReply reply;
        
        if (client_to_3->sendRequestVote(args, reply, 1000)) {
            std::cout << "   Node 3 response: term=" << reply.term 
                      << ", voteGranted=" << (reply.voteGranted ? "true" : "false") << std::endl;
            
            // 处理投票响应
            if (node1->handleVoteResponse(3, reply)) {
                std::cout << "   ✓ Node 1 became Leader!" << std::endl;
            }
        }
        client_to_3->disconnect();
    }
    
    // 节点 1 作为 Leader 发送心跳
    if (node1->isLeader()) {
        std::cout << "\n4. Node 1 (Leader) sending heartbeats..." << std::endl;
        
        // 向节点 2 发送心跳
        if (client_to_2->connect("127.0.0.1:8002")) {
            raft::AppendEntriesArgs args = node1->createAppendEntriesArgs(0);  // 节点 2 是 peers[0]
            raft::AppendEntriesReply reply;
            
            if (client_to_2->sendAppendEntries(args, reply, 1000)) {
                std::cout << "   Node 2 heartbeat response: term=" << reply.term 
                          << ", success=" << (reply.success ? "true" : "false") << std::endl;
            }
            client_to_2->disconnect();
        }
        
        // 向节点 3 发送心跳
        if (client_to_3->connect("127.0.0.1:8003")) {
            raft::AppendEntriesArgs args = node1->createAppendEntriesArgs(1);  // 节点 3 是 peers[1]
            raft::AppendEntriesReply reply;
            
            if (client_to_3->sendAppendEntries(args, reply, 1000)) {
                std::cout << "   Node 3 heartbeat response: term=" << reply.term 
                          << ", success=" << (reply.success ? "true" : "false") << std::endl;
            }
            client_to_3->disconnect();
        }
        
        std::cout << "   ✓ Heartbeats sent successfully" << std::endl;
    }
    
    // 演示超时和重试
    std::cout << "\n5. Testing timeout and retry..." << std::endl;
    auto client_to_invalid = network::createRPCClient("127.0.0.1:9999");
    client_to_invalid->setRetries(2);
    
    raft::RequestVoteArgs args(1, 1, 0, 0);
    raft::RequestVoteReply reply;
    
    auto start = std::chrono::steady_clock::now();
    bool success = client_to_invalid->sendRequestVote(args, reply, 500);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "   Request to invalid server: " 
              << (success ? "succeeded" : "failed") 
              << " (took " << duration.count() << "ms)" << std::endl;
    
    // 停止服务器
    std::cout << "\n6. Stopping servers..." << std::endl;
    server1->stop();
    server2->stop();
    server3->stop();
    std::cout << "   ✓ All servers stopped" << std::endl;
    
    std::cout << "\n=== Demo completed successfully! ===" << std::endl;
    
    return 0;
}
