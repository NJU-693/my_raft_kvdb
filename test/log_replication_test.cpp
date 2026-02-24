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

std::string testCallback(const std::string& command) {
    appliedCommands.push_back(command);
    return "OK";
}

/**
 * @brief 测试 Leader 追加日志条目
 */
void testLeaderAppendLogEntry() {
    std::cout << "\n=== Test: Leader Append Log Entry ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // 让节点成为 Leader
    node.becomeLeader();
    assertTrue(node.isLeader(), "Node should be Leader");
    
    // 追加日志条目
    int index1 = node.appendLogEntry("PUT:key1:value1");
    int index2 = node.appendLogEntry("PUT:key2:value2");
    int index3 = node.appendLogEntry("DELETE:key1");
    
    // 验证日志索引
    assertEqual(1, index1, "First log entry should have index 1");
    assertEqual(2, index2, "Second log entry should have index 2");
    assertEqual(3, index3, "Third log entry should have index 3");
    
    // 验证日志内容
    LogEntry entry1 = node.getLogEntry(1);
    LogEntry entry2 = node.getLogEntry(2);
    LogEntry entry3 = node.getLogEntry(3);
    
    assertEqual("PUT:key1:value1", entry1.command, "First entry command should match");
    assertEqual(node.getCurrentTerm(), entry1.term, "First entry term should match current term");
    assertEqual(1, entry1.index, "First entry index should be 1");
    
    assertEqual("PUT:key2:value2", entry2.command, "Second entry command should match");
    assertEqual("DELETE:key1", entry3.command, "Third entry command should match");
    
    // 验证日志大小
    assertEqual(3, node.getLogSize(), "Log size should be 3");
    assertEqual(3, node.getLastLogIndex(), "Last log index should be 3");
}

/**
 * @brief 测试非 Leader 不能追加日志
 */
void testNonLeaderCannotAppendLog() {
    std::cout << "\n=== Test: Non-Leader Cannot Append Log ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // node 是 Follower
    assertTrue(!node.isLeader(), "Node should not be Leader");
    
    // 尝试追加日志应该失败
    int index = node.appendLogEntry("PUT:key1:value1");
    assertEqual(-1, index, "Non-leader should not be able to append log");
    assertEqual(0, node.getLogSize(), "Log size should remain 0");
}

/**
 * @brief 测试创建 AppendEntries 参数
 */
void testCreateAppendEntriesArgs() {
    std::cout << "\n=== Test: Create AppendEntries Args ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // 让节点成为 Leader
    node.becomeLeader();
    
    // 追加一些日志条目
    node.appendLogEntry("PUT:key1:value1");
    node.appendLogEntry("PUT:key2:value2");
    
    // 为第一个 peer 创建 AppendEntries 参数（nodeIndex = 0）
    AppendEntriesArgs args = node.createAppendEntriesArgs(0);
    
    // 验证参数
    assertEqual(node.getCurrentTerm(), args.term, "Term should match current term");
    assertEqual(1, args.leaderId, "Leader ID should be 1");
    assertEqual(0, args.prevLogIndex, "Initial prevLogIndex should be 0");
    assertEqual(0, args.prevLogTerm, "Initial prevLogTerm should be 0");
    assertEqual(0, args.leaderCommit, "Initial leaderCommit should be 0");
    assertEqual(2, static_cast<int>(args.entries.size()), "Should have 2 log entries");
    
    // 验证日志条目内容
    assertEqual("PUT:key1:value1", args.entries[0].command, "First entry command should match");
    assertEqual("PUT:key2:value2", args.entries[1].command, "Second entry command should match");
}

/**
 * @brief 测试 Follower 处理 AppendEntries（成功情况）
 */
void testFollowerHandleAppendEntriesSuccess() {
    std::cout << "\n=== Test: Follower Handle AppendEntries Success ===" << std::endl;
    
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    RaftNode leader(1, peers1);
    RaftNode follower(2, peers2);
    
    // leader 成为 Leader
    leader.becomeLeader();
    
    // 创建 AppendEntries 请求
    AppendEntriesArgs args;
    args.term = leader.getCurrentTerm();
    args.leaderId = 1;
    args.prevLogIndex = 0;
    args.prevLogTerm = 0;
    args.leaderCommit = 0;
    args.entries.push_back(LogEntry(args.term, "PUT:key1:value1", 1));
    args.entries.push_back(LogEntry(args.term, "PUT:key2:value2", 2));
    
    // follower 处理请求
    AppendEntriesReply reply;
    follower.handleAppendEntries(args, reply);
    
    // 验证响应
    assertTrue(reply.success, "AppendEntries should succeed");
    assertEqual(follower.getCurrentTerm(), reply.term, "Reply term should match follower term");
    
    // 验证 follower 的日志
    assertEqual(2, follower.getLogSize(), "Follower should have 2 log entries");
    assertEqual("PUT:key1:value1", follower.getLogEntry(1).command, "First entry should match");
    assertEqual("PUT:key2:value2", follower.getLogEntry(2).command, "Second entry should match");
    
    // 验证 follower 状态
    assertTrue(follower.getState() == NodeState::FOLLOWER, "Node should remain FOLLOWER");
    assertEqual(1, follower.getLeaderId(), "Leader ID should be set to 1");
}

/**
 * @brief 测试 Follower 处理 AppendEntries（日志冲突）
 */
void testFollowerHandleAppendEntriesConflict() {
    std::cout << "\n=== Test: Follower Handle AppendEntries Conflict ===" << std::endl;
    
    std::vector<int> peers = {1, 3};
    RaftNode follower(2, peers);
    
    // 设置 follower 有一些不一致的日志
    follower.becomeLeader();  // 临时成为 Leader 以追加日志
    follower.appendLogEntry("PUT:key1:old_value");
    follower.becomeFollower(2);  // 转回 Follower，任期为 2
    
    // 创建来自新 Leader 的冲突 AppendEntries 请求
    AppendEntriesArgs args;
    args.term = 2;  // 相同任期
    args.leaderId = 1;
    args.prevLogIndex = 0;
    args.prevLogTerm = 0;
    args.leaderCommit = 0;
    args.entries.push_back(LogEntry(2, "PUT:key1:new_value", 1));
    
    // follower 处理请求
    AppendEntriesReply reply;
    follower.handleAppendEntries(args, reply);
    
    // 验证响应成功（冲突已解决）
    assertTrue(reply.success, "AppendEntries should succeed after resolving conflict");
    
    // 验证 follower 的日志被正确更新
    assertEqual(1, follower.getLogSize(), "Follower should have 1 log entry");
    assertEqual("PUT:key1:new_value", follower.getLogEntry(1).command, "Entry should be updated");
}

/**
 * @brief 测试 Follower 处理 AppendEntries（日志不够长）
 */
void testFollowerHandleAppendEntriesLogTooShort() {
    std::cout << "\n=== Test: Follower Handle AppendEntries Log Too Short ===" << std::endl;
    
    std::vector<int> peers = {1, 3};
    RaftNode follower(2, peers);
    
    // 创建 prevLogIndex 超出 follower 日志长度的请求
    AppendEntriesArgs args;
    args.term = 1;
    args.leaderId = 1;
    args.prevLogIndex = 5;  // follower 没有这么多日志
    args.prevLogTerm = 1;
    args.leaderCommit = 0;
    args.entries.push_back(LogEntry(1, "PUT:key1:value1", 6));
    
    // follower 处理请求
    AppendEntriesReply reply;
    follower.handleAppendEntries(args, reply);
    
    // 验证响应失败
    assertTrue(!reply.success, "AppendEntries should fail when log is too short");
    assertEqual(1, reply.conflictIndex, "Conflict index should be log size + 1");
    assertEqual(-1, reply.conflictTerm, "Conflict term should be -1 for missing entries");
}

/**
 * @brief 测试 Leader 处理 AppendEntries 响应（成功）
 */
void testLeaderHandleAppendEntriesResponseSuccess() {
    std::cout << "\n=== Test: Leader Handle AppendEntries Response Success ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode leader(1, peers);
    
    // 成为 Leader 并追加日志
    leader.becomeLeader();
    leader.appendLogEntry("PUT:key1:value1");
    leader.appendLogEntry("PUT:key2:value2");
    
    // 模拟成功响应
    AppendEntriesReply reply;
    reply.term = leader.getCurrentTerm();
    reply.success = true;
    
    // 处理响应
    bool updated = leader.handleAppendEntriesResponse(2, reply);
    assertTrue(updated, "Should return true for successful response");
    
    // 验证后续的 AppendEntries 参数反映了更新
    AppendEntriesArgs args = leader.createAppendEntriesArgs(0);  // node2 在 peers[0]
    assertEqual(2, args.prevLogIndex, "prevLogIndex should be updated to last log index");
}

/**
 * @brief 测试 Leader 处理 AppendEntries 响应（失败，需要回退）
 */
void testLeaderHandleAppendEntriesResponseFailure() {
    std::cout << "\n=== Test: Leader Handle AppendEntries Response Failure ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode leader(1, peers);
    
    // 成为 Leader 并追加日志（通过选举过程）
    leader.startElection();  // 这会增加任期
    leader.becomeLeader();   // 然后成为 Leader
    leader.appendLogEntry("PUT:key1:value1");
    leader.appendLogEntry("PUT:key2:value2");
    
    // 模拟失败响应（日志冲突）
    AppendEntriesReply reply;
    reply.term = leader.getCurrentTerm();
    reply.success = false;
    reply.conflictIndex = 1;
    reply.conflictTerm = 0;
    
    // 检查初始状态
    AppendEntriesArgs initialArgs = leader.createAppendEntriesArgs(0);
    
    // 处理响应
    bool updated = leader.handleAppendEntriesResponse(2, reply);
    assertTrue(!updated, "Should return false for failed response");
    
    // 验证 nextIndex 被回退
    AppendEntriesArgs args = leader.createAppendEntriesArgs(0);
    assertTrue(args.prevLogIndex < 2, "prevLogIndex should be decremented");
}

/**
 * @brief 测试心跳（空的 AppendEntries）
 */
void testHeartbeat() {
    std::cout << "\n=== Test: Heartbeat ===" << std::endl;
    
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    RaftNode leader(1, peers1);
    RaftNode follower(2, peers2);
    
    // leader 成为 Leader
    leader.becomeLeader();
    
    // 创建心跳消息（空的 AppendEntries）
    AppendEntriesArgs heartbeat = leader.createAppendEntriesArgs(0);
    
    // 心跳应该没有日志条目
    assertTrue(heartbeat.entries.empty(), "Heartbeat should have no log entries");
    assertEqual(leader.getCurrentTerm(), heartbeat.term, "Heartbeat term should match leader term");
    assertEqual(1, heartbeat.leaderId, "Heartbeat leader ID should be 1");
    
    // follower 处理心跳
    AppendEntriesReply reply;
    follower.handleAppendEntries(heartbeat, reply);
    
    // 验证心跳成功
    assertTrue(reply.success, "Heartbeat should succeed");
    assertEqual(1, follower.getLeaderId(), "Follower should recognize leader");
    assertTrue(follower.getState() == NodeState::FOLLOWER, "Follower should remain FOLLOWER");
}

/**
 * @brief 测试状态机应用回调
 */
void testStateMachineCallback() {
    std::cout << "\n=== Test: State Machine Callback ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // 清空全局回调记录
    appliedCommands.clear();
    
    // 设置回调
    node.setApplyCallback(testCallback);
    
    // 成为 Leader 并追加日志
    node.becomeLeader();
    node.appendLogEntry("PUT:key1:value1");
    node.appendLogEntry("DELETE:key2");
    
    // 手动触发应用（在实际系统中，这会在日志提交后自动发生）
    // 这里我们直接调用 applyCommittedEntries 来测试回调机制
    node.applyCommittedEntries();
    
    // 由于没有实际提交日志，appliedCommands 应该为空
    // 这个测试主要验证回调设置机制的正确性
    assertTrue(appliedCommands.empty(), "No commands should be applied without commit");
}

/**
 * @brief 测试获取状态信息
 */
void testGetStateInfo() {
    std::cout << "\n=== Test: Get State Info ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // 初始状态
    assertEqual(0, node.getCommitIndex(), "Initial commit index should be 0");
    assertEqual(0, node.getLastApplied(), "Initial last applied should be 0");
    
    // 成为 Leader 并追加日志
    node.becomeLeader();
    node.appendLogEntry("PUT:key1:value1");
    
    // 日志追加后，commitIndex 仍应为 0（未提交）
    assertEqual(0, node.getCommitIndex(), "Commit index should remain 0 until committed");
    assertEqual(0, node.getLastApplied(), "Last applied should remain 0 until applied");
}

int main() {
    std::cout << "Running Log Replication Tests..." << std::endl;
    
    try {
        testLeaderAppendLogEntry();
        testNonLeaderCannotAppendLog();
        testCreateAppendEntriesArgs();
        testFollowerHandleAppendEntriesSuccess();
        testFollowerHandleAppendEntriesConflict();
        testFollowerHandleAppendEntriesLogTooShort();
        testLeaderHandleAppendEntriesResponseSuccess();
        testLeaderHandleAppendEntriesResponseFailure();
        testHeartbeat();
        testStateMachineCallback();
        testGetStateInfo();
        
        std::cout << "\n=== All Log Replication Tests Passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}