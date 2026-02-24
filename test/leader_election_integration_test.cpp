#include "raft/raft_node.h"
#include <iostream>
#include <vector>
#include <memory>
#include <cassert>
#include <thread>
#include <chrono>

using namespace raft;

// 测试辅助函数
void assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        exit(1);
    }
    std::cout << "PASSED: " << message << std::endl;
}

/**
 * @brief 模拟完整的领导者选举过程
 * 
 * 这个测试演示了一个完整的选举周期：
 * 1. 节点启动为 Follower
 * 2. 选举超时后成为 Candidate
 * 3. 发送 RequestVote RPC 给其他节点
 * 4. 收集投票响应
 * 5. 获得多数票后成为 Leader
 */
void testCompleteElectionWorkflow() {
    std::cout << "\n=== Integration Test: Complete Election Workflow ===" << std::endl;
    
    // 创建 3 节点集群
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    std::vector<int> peers3 = {1, 2};
    
    auto node1 = std::make_unique<RaftNode>(1, peers1);
    auto node2 = std::make_unique<RaftNode>(2, peers2);
    auto node3 = std::make_unique<RaftNode>(3, peers3);
    
    // 验证初始状态
    assertTrue(node1->getState() == NodeState::FOLLOWER, "Node 1 should start as FOLLOWER");
    assertTrue(node2->getState() == NodeState::FOLLOWER, "Node 2 should start as FOLLOWER");
    assertTrue(node3->getState() == NodeState::FOLLOWER, "Node 3 should start as FOLLOWER");
    
    std::cout << "Step 1: All nodes start as FOLLOWER" << std::endl;
    
    // 模拟节点 1 选举超时，发起选举
    node1->startElection();
    assertTrue(node1->getState() == NodeState::CANDIDATE, "Node 1 should become CANDIDATE");
    assertTrue(node1->getCurrentTerm() == 1, "Node 1 term should be 1");
    assertTrue(node1->getVoteCount() == 1, "Node 1 should have 1 vote (self)");
    
    std::cout << "Step 2: Node 1 starts election (term 1)" << std::endl;
    
    // 构造 RequestVote 请求
    RequestVoteArgs voteRequest(
        node1->getCurrentTerm(),    // term
        1,                          // candidateId
        node1->getLastLogIndex(),   // lastLogIndex
        node1->getLastLogTerm()     // lastLogTerm
    );
    
    // 节点 2 和节点 3 处理投票请求
    RequestVoteReply reply2, reply3;
    node2->handleRequestVote(voteRequest, reply2);
    node3->handleRequestVote(voteRequest, reply3);
    
    assertTrue(reply2.voteGranted, "Node 2 should grant vote to Node 1");
    assertTrue(reply3.voteGranted, "Node 3 should grant vote to Node 1");
    assertTrue(reply2.term == 1, "Node 2 reply term should be 1");
    assertTrue(reply3.term == 1, "Node 3 reply term should be 1");
    
    std::cout << "Step 3: Node 2 and Node 3 grant votes to Node 1" << std::endl;
    
    // 节点 1 处理投票响应
    bool becameLeader2 = node1->handleVoteResponse(2, reply2);
    assertTrue(becameLeader2, "Node 1 should become leader after receiving vote from Node 2");
    assertTrue(node1->getState() == NodeState::LEADER, "Node 1 should be LEADER");
    assertTrue(node1->isLeader(), "Node 1 isLeader() should return true");
    
    std::cout << "Step 4: Node 1 becomes LEADER with majority votes (2/3)" << std::endl;
    
    // 验证其他节点状态（它们应该在收到 RequestVote 时更新任期并保持 Follower）
    assertTrue(node2->getCurrentTerm() == 1, "Node 2 should update term to 1");
    assertTrue(node3->getCurrentTerm() == 1, "Node 3 should update term to 1");
    assertTrue(node2->getState() == NodeState::FOLLOWER, "Node 2 should remain FOLLOWER");
    assertTrue(node3->getState() == NodeState::FOLLOWER, "Node 3 should remain FOLLOWER");
    
    std::cout << "Step 5: Node 2 and Node 3 remain as FOLLOWER with updated term" << std::endl;
    
    // 模拟 Leader 发送心跳
    assertTrue(node1->shouldSendHeartbeat() == false, "Leader should not need immediate heartbeat");
    
    // 等待心跳间隔
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    assertTrue(node1->shouldSendHeartbeat(), "Leader should need to send heartbeat after interval");
    
    std::cout << "Step 6: Leader heartbeat mechanism working correctly" << std::endl;
    
    std::cout << "✓ Complete election workflow successful!" << std::endl;
}

/**
 * @brief 测试分裂投票和重新选举场景
 */
void testSplitVoteAndReelection() {
    std::cout << "\n=== Integration Test: Split Vote and Re-election ===" << std::endl;
    
    // 创建 3 节点集群
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    std::vector<int> peers3 = {1, 2};
    
    auto node1 = std::make_unique<RaftNode>(1, peers1);
    auto node2 = std::make_unique<RaftNode>(2, peers2);
    auto node3 = std::make_unique<RaftNode>(3, peers3);
    
    // 模拟同时发起选举（分裂投票场景）
    node1->startElection();  // term 1
    node2->startElection();  // term 1
    
    assertTrue(node1->getCurrentTerm() == 1, "Node 1 term should be 1");
    assertTrue(node2->getCurrentTerm() == 1, "Node 2 term should be 1");
    
    std::cout << "Step 1: Node 1 and Node 2 start elections simultaneously" << std::endl;
    
    // 构造投票请求
    RequestVoteArgs request1(1, 1, 0, 0);  // Node 1's request
    RequestVoteArgs request2(1, 2, 0, 0);  // Node 2's request
    
    // Node 3 先收到 Node 1 的请求并投票
    RequestVoteReply reply31;
    node3->handleRequestVote(request1, reply31);
    assertTrue(reply31.voteGranted, "Node 3 should grant vote to Node 1");
    
    // Node 3 再收到 Node 2 的请求，应该拒绝（已经投票给 Node 1）
    RequestVoteReply reply32;
    node3->handleRequestVote(request2, reply32);
    assertTrue(!reply32.voteGranted, "Node 3 should reject vote for Node 2 (already voted)");
    
    std::cout << "Step 2: Node 3 votes for Node 1, rejects Node 2" << std::endl;
    
    // Node 1 和 Node 2 互相拒绝对方的投票请求
    RequestVoteReply reply12, reply21;
    node1->handleRequestVote(request2, reply12);
    node2->handleRequestVote(request1, reply21);
    
    assertTrue(!reply12.voteGranted, "Node 1 should reject Node 2 (already voted for self)");
    assertTrue(!reply21.voteGranted, "Node 2 should reject Node 1 (already voted for self)");
    
    std::cout << "Step 3: Node 1 and Node 2 reject each other's votes" << std::endl;
    
    // Node 1 处理投票响应，应该成为 Leader（获得 2/3 票：自己 + Node 3）
    bool becameLeader = node1->handleVoteResponse(3, reply31);
    assertTrue(becameLeader, "Node 1 should become leader with 2/3 votes");
    assertTrue(node1->getState() == NodeState::LEADER, "Node 1 should be LEADER");
    
    // Node 2 处理投票响应，应该保持 Candidate（只有自己的票）
    becameLeader = node2->handleVoteResponse(1, reply21);
    assertTrue(!becameLeader, "Node 2 should not become leader with only 1/3 votes");
    assertTrue(node2->getState() == NodeState::CANDIDATE, "Node 2 should remain CANDIDATE");
    
    std::cout << "Step 4: Node 1 becomes LEADER, Node 2 remains CANDIDATE" << std::endl;
    
    std::cout << "✓ Split vote scenario handled correctly!" << std::endl;
}

int main() {
    std::cout << "Running Leader Election Integration Tests..." << std::endl;
    
    try {
        testCompleteElectionWorkflow();
        testSplitVoteAndReelection();
        
        std::cout << "\n=== All Integration Tests Passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nIntegration test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}