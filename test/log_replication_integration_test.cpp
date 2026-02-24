#include "raft/raft_node.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <memory>

using namespace raft;

// 测试辅助函数
void assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        exit(1);
    }
    std::cout << "PASSED: " << message << std::endl;
}

void assertEqual(int expected, int actual, const std::string& message) {
    if (expected != actual) {
        std::cerr << "FAILED: " << message << " (expected: " << expected 
                  << ", actual: " << actual << ")" << std::endl;
        exit(1);
    }
    std::cout << "PASSED: " << message << std::endl;
}

void assertEqual(const std::string& expected, const std::string& actual, const std::string& message) {
    if (expected != actual) {
        std::cerr << "FAILED: " << message << " (expected: '" << expected 
                  << "', actual: '" << actual << "')" << std::endl;
        exit(1);
    }
    std::cout << "PASSED: " << message << std::endl;
}

// 全局变量用于测试回调
std::vector<std::string> appliedCommands;

void testCallback(const std::string& command) {
    appliedCommands.push_back(command);
}

/**
 * @brief 测试完整的日志复制流程
 * 
 * 模拟一个 3 节点集群的日志复制过程：
 * 1. Leader 追加日志
 * 2. 向 Follower 发送 AppendEntries
 * 3. 收到多数响应后提交日志
 * 4. 应用日志到状态机
 */
void testCompleteLogReplicationFlow() {
    std::cout << "\n=== Test: Complete Log Replication Flow ===" << std::endl;
    
    // 创建 3 节点集群
    std::vector<int> peers1 = {2, 3};  // node1 的 peers
    std::vector<int> peers2 = {1, 3};  // node2 的 peers  
    std::vector<int> peers3 = {1, 2};  // node3 的 peers
    
    RaftNode leader(1, peers1);
    RaftNode follower1(2, peers2);
    RaftNode follower2(3, peers3);
    
    // 清空回调记录
    appliedCommands.clear();
    
    // 设置状态机回调
    leader.setApplyCallback(testCallback);
    
    // node1 成为 Leader
    leader.startElection();
    leader.becomeLeader();
    
    // Leader 追加日志条目
    int index1 = leader.appendLogEntry("PUT:key1:value1");
    int index2 = leader.appendLogEntry("PUT:key2:value2");
    
    assertEqual(1, index1, "First log entry should have index 1");
    assertEqual(2, index2, "Second log entry should have index 2");
    assertEqual(0, leader.getCommitIndex(), "Initial commit index should be 0");
    
    // 模拟向 follower1 发送 AppendEntries
    AppendEntriesArgs args1 = leader.createAppendEntriesArgs(0);  // follower1 在 peers[0]
    AppendEntriesReply reply1;
    follower1.handleAppendEntries(args1, reply1);
    
    assertTrue(reply1.success, "Follower1 should accept AppendEntries");
    assertEqual(2, follower1.getLogSize(), "Follower1 should have 2 log entries");
    
    // Leader 处理 follower1 的响应
    bool updated1 = leader.handleAppendEntriesResponse(2, reply1);
    assertTrue(updated1, "Leader should update state after successful response");
    
    // 此时有 Leader + 1 follower = 2 节点，在 3 节点集群中已达到多数，应该提交
    assertEqual(2, leader.getCommitIndex(), "Should commit with majority (leader + 1 follower)");
    
    // 模拟向 follower2 发送 AppendEntries
    AppendEntriesArgs args2 = leader.createAppendEntriesArgs(1);  // follower2 在 peers[1]
    AppendEntriesReply reply2;
    follower2.handleAppendEntries(args2, reply2);
    
    assertTrue(reply2.success, "Follower2 should accept AppendEntries");
    assertEqual(2, follower2.getLogSize(), "Follower2 should have 2 log entries");
    
    // Leader 处理 follower2 的响应
    bool updated2 = leader.handleAppendEntriesResponse(3, reply2);
    assertTrue(updated2, "Leader should update state after successful response");
    
    // 仍然应该是已提交状态（没有变化）
    assertEqual(2, leader.getCommitIndex(), "Commit index should remain 2");
    
    // 应用已提交的日志
    leader.applyCommittedEntries();
    
    // 验证状态机回调被调用
    assertEqual(2, static_cast<int>(appliedCommands.size()), "Should apply 2 commands");
    assertEqual("PUT:key1:value1", appliedCommands[0], "First applied command should match");
    assertEqual("PUT:key2:value2", appliedCommands[1], "Second applied command should match");
    assertEqual(2, leader.getLastApplied(), "Last applied index should be 2");
}

/**
 * @brief 测试日志冲突解决
 * 
 * 模拟 Follower 日志太短的情况，验证回退机制
 */
void testLogConflictResolution() {
    std::cout << "\n=== Test: Log Conflict Resolution ===" << std::endl;
    
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    
    RaftNode leader(1, peers1);
    RaftNode follower(2, peers2);
    
    // 先让 Leader 追加日志，然后成为 Leader
    // 这样 nextIndex 会初始化为 lastLogIndex + 1
    leader.startElection();
    leader.becomeLeader();
    leader.appendLogEntry("PUT:key1:value1");
    leader.appendLogEntry("PUT:key2:value2");
    leader.appendLogEntry("PUT:key3:value3");
    
    // 重新成为 Leader 以正确初始化 nextIndex
    int term = leader.getCurrentTerm();
    leader.becomeFollower(term + 1);
    leader.startElection();
    leader.becomeLeader();
    
    // Follower 只有部分日志
    follower.startElection();
    follower.becomeLeader();
    follower.appendLogEntry("PUT:key1:value1");  // 相同的第一个条目
    follower.becomeFollower(leader.getCurrentTerm());
    
    // 验证初始状态
    assertEqual(3, leader.getLogSize(), "Leader should have 3 log entries");
    assertEqual(1, follower.getLogSize(), "Follower should have 1 log entry");
    
    // Leader 向 Follower 发送 AppendEntries
    // 此时 nextIndex[0] = 4（因为 leader 重新成为 Leader 时有 3 个日志）
    AppendEntriesArgs args = leader.createAppendEntriesArgs(0);
    AppendEntriesReply reply;
    follower.handleAppendEntries(args, reply);
    
    // 第一次应该失败（follower 的日志太短）
    assertTrue(!reply.success, "First AppendEntries should fail - follower log too short");
    assertTrue(reply.conflictIndex > 0, "Should report conflict index");
    
    // Leader 处理失败响应，回退 nextIndex
    leader.handleAppendEntriesResponse(2, reply);
    
    // 再次发送 AppendEntries（从更早的位置开始）
    AppendEntriesArgs args2 = leader.createAppendEntriesArgs(0);
    AppendEntriesReply reply2;
    follower.handleAppendEntries(args2, reply2);
    
    // 这次应该成功
    assertTrue(reply2.success, "Second AppendEntries should succeed");
    assertEqual(3, follower.getLogSize(), "Follower should have 3 log entries after resolution");
    
    // 验证 Follower 的日志与 Leader 一致
    LogEntry entry1 = follower.getLogEntry(1);
    LogEntry entry2 = follower.getLogEntry(2);
    LogEntry entry3 = follower.getLogEntry(3);
    assertEqual("PUT:key1:value1", entry1.command, "First entry should match leader");
    assertEqual("PUT:key2:value2", entry2.command, "Second entry should match leader");
    assertEqual("PUT:key3:value3", entry3.command, "Third entry should match leader");
}

/**
 * @brief 测试心跳机制
 * 
 * 验证 Leader 发送心跳维持领导地位
 */
void testHeartbeatMechanism() {
    std::cout << "\n=== Test: Heartbeat Mechanism ===" << std::endl;
    
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    
    RaftNode leader(1, peers1);
    RaftNode follower(2, peers2);
    
    // Leader 成为 Leader
    leader.startElection();
    leader.becomeLeader();
    
    // 创建心跳消息（空的 AppendEntries）
    AppendEntriesArgs heartbeat = leader.createAppendEntriesArgs(0);
    
    // 心跳应该没有日志条目
    assertTrue(heartbeat.entries.empty(), "Heartbeat should have no log entries");
    assertEqual(leader.getCurrentTerm(), heartbeat.term, "Heartbeat term should match leader term");
    assertEqual(1, heartbeat.leaderId, "Heartbeat leader ID should be 1");
    assertEqual(0, heartbeat.prevLogIndex, "Heartbeat prevLogIndex should be 0");
    assertEqual(0, heartbeat.leaderCommit, "Heartbeat leaderCommit should be 0");
    
    // Follower 处理心跳
    AppendEntriesReply reply;
    follower.handleAppendEntries(heartbeat, reply);
    
    // 验证心跳成功
    assertTrue(reply.success, "Heartbeat should succeed");
    assertEqual(leader.getCurrentTerm(), reply.term, "Reply term should match");
    assertEqual(1, follower.getLeaderId(), "Follower should recognize leader");
    assertTrue(follower.getState() == NodeState::FOLLOWER, "Follower should remain FOLLOWER");
    
    // 验证 Follower 的选举超时被重置（通过检查它不会立即超时）
    assertTrue(!follower.checkElectionTimeout(), "Election timeout should be reset after heartbeat");
}

int main() {
    std::cout << "Running Log Replication Integration Tests..." << std::endl;
    
    try {
        testCompleteLogReplicationFlow();
        testLogConflictResolution();
        testHeartbeatMechanism();
        
        std::cout << "\n=== All Log Replication Integration Tests Passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}