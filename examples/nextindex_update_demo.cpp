/**
 * @file nextindex_update_demo.cpp
 * @brief 演示真实 Raft 系统中 nextIndex 的更新机制
 * 
 * 这个示例展示了在实际运行中，nextIndex 是如何通过 AppendEntries
 * 响应处理动态更新的，而不需要手动调用 becomeLeader()。
 */

#include "raft/raft_node.h"
#include <iostream>
#include <iomanip>

using namespace raft;

// 辅助函数：打印节点状态
void printNodeState(const std::string& label, const RaftNode& node, int followerIndex = -1) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << label << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "  State: " << (node.isLeader() ? "LEADER" : "FOLLOWER") << std::endl;
    std::cout << "  Term: " << node.getCurrentTerm() << std::endl;
    std::cout << "  Log Size: " << node.getLogSize() << std::endl;
    std::cout << "  Commit Index: " << node.getCommitIndex() << std::endl;
    
    if (node.isLeader() && followerIndex >= 0) {
        // 注意：在实际代码中 nextIndex 是私有的，这里只是概念演示
        std::cout << "  [假设] nextIndex[" << followerIndex << "]: 通过响应处理更新" << std::endl;
    }
}

// 辅助函数：打印 AppendEntries 参数
void printAppendEntriesArgs(const AppendEntriesArgs& args) {
    std::cout << "\n📤 AppendEntries RPC:" << std::endl;
    std::cout << "  term: " << args.term << std::endl;
    std::cout << "  leaderId: " << args.leaderId << std::endl;
    std::cout << "  prevLogIndex: " << args.prevLogIndex << std::endl;
    std::cout << "  prevLogTerm: " << args.prevLogTerm << std::endl;
    std::cout << "  leaderCommit: " << args.leaderCommit << std::endl;
    std::cout << "  entries.size(): " << args.entries.size() << std::endl;
    
    for (size_t i = 0; i < args.entries.size(); ++i) {
        std::cout << "    [" << i << "] index=" << args.entries[i].index 
                  << ", term=" << args.entries[i].term
                  << ", cmd=" << args.entries[i].command << std::endl;
    }
}

/**
 * @brief 演示场景1：正常的日志复制流程
 * 
 * 展示 nextIndex 如何在成功复制后自动更新
 */
void demoNormalReplication() {
    std::cout << "\n\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  场景1：正常的日志复制流程（nextIndex 动态更新）          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    
    RaftNode leader(1, peers1);
    RaftNode follower(2, peers2);
    
    // === 步骤1：node1 成为 Leader ===
    std::cout << "\n【步骤1】node1 成为 Leader" << std::endl;
    leader.startElection();
    leader.becomeLeader();
    printNodeState("Leader (node1) 状态", leader, 0);
    std::cout << "  💡 此时 nextIndex[0] = 1 (lastLogIndex + 1 = 0 + 1)" << std::endl;
    
    // === 步骤2：Leader 追加日志 ===
    std::cout << "\n【步骤2】Leader 接收客户端请求，追加日志" << std::endl;
    leader.appendLogEntry("SET:x:1");
    leader.appendLogEntry("SET:y:2");
    leader.appendLogEntry("SET:z:3");
    printNodeState("Leader 追加日志后", leader, 0);
    std::cout << "  💡 注意：nextIndex[0] 仍然是 1，不会自动更新！" << std::endl;
    
    // === 步骤3：Leader 创建 AppendEntries RPC（第一次）===
    std::cout << "\n【步骤3】Leader 创建 AppendEntries RPC" << std::endl;
    AppendEntriesArgs args1 = leader.createAppendEntriesArgs(0);
    printAppendEntriesArgs(args1);
    std::cout << "  💡 因为 nextIndex[0]=1，所以 prevLogIndex=0，发送所有3个日志" << std::endl;
    
    // === 步骤4：Follower 处理 AppendEntries ===
    std::cout << "\n【步骤4】Follower 处理 AppendEntries" << std::endl;
    AppendEntriesReply reply1;
    follower.handleAppendEntries(args1, reply1);
    printNodeState("Follower 接收日志后", follower);
    std::cout << "  📥 Reply: success=" << (reply1.success ? "true" : "false") 
              << ", term=" << reply1.term << std::endl;
    
    // === 步骤5：Leader 处理响应（关键：更新 nextIndex）===
    std::cout << "\n【步骤5】Leader 处理响应（🔑 关键步骤！）" << std::endl;
    bool updated = leader.handleAppendEntriesResponse(2, reply1);
    std::cout << "  ✅ 响应处理结果: " << (updated ? "成功" : "失败") << std::endl;
    std::cout << "  🔄 nextIndex[0] 被更新为: lastLogIndex + 1 = 3 + 1 = 4" << std::endl;
    std::cout << "  🔄 matchIndex[0] 被更新为: 3" << std::endl;
    printNodeState("Leader 处理响应后", leader, 0);
    
    // === 步骤6：Leader 追加更多日志 ===
    std::cout << "\n【步骤6】Leader 追加第4个日志" << std::endl;
    leader.appendLogEntry("SET:w:4");
    printNodeState("Leader 追加新日志后", leader, 0);
    std::cout << "  💡 nextIndex[0] 仍然是 4（不变）" << std::endl;
    
    // === 步骤7：下次复制只发送新日志 ===
    std::cout << "\n【步骤7】Leader 创建下一次 AppendEntries RPC" << std::endl;
    AppendEntriesArgs args2 = leader.createAppendEntriesArgs(0);
    printAppendEntriesArgs(args2);
    std::cout << "  💡 因为 nextIndex[0]=4，所以 prevLogIndex=3，只发送新的第4个日志" << std::endl;
    
    // === 步骤8：再次成功复制 ===
    std::cout << "\n【步骤8】Follower 接收并成功复制" << std::endl;
    AppendEntriesReply reply2;
    follower.handleAppendEntries(args2, reply2);
    leader.handleAppendEntriesResponse(2, reply2);
    std::cout << "  🔄 nextIndex[0] 被更新为: 5" << std::endl;
    std::cout << "  🔄 matchIndex[0] 被更新为: 4" << std::endl;
    
    std::cout << "\n【总结】" << std::endl;
    std::cout << "  ✅ nextIndex 通过 handleAppendEntriesResponse() 自动更新" << std::endl;
    std::cout << "  ✅ 不需要重新调用 becomeLeader()" << std::endl;
    std::cout << "  ✅ 形成自适应的日志复制反馈循环" << std::endl;
}

/**
 * @brief 演示场景2：Follower 日志太短的冲突解决
 * 
 * 展示 nextIndex 如何在失败后回退并重试
 */
void demoConflictResolution() {
    std::cout << "\n\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  场景2：日志冲突解决（nextIndex 回退机制）                ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    std::vector<int> peers1 = {2, 3};
    std::vector<int> peers2 = {1, 3};
    
    RaftNode leader(1, peers1);
    RaftNode follower(2, peers2);
    
    // === 构造场景：Leader 有日志，Follower 启动为空 ===
    std::cout << "\n【初始化】构造测试场景" << std::endl;
    
    // Leader 先成为 Leader 并追加日志
    leader.startElection();
    leader.becomeLeader();
    leader.appendLogEntry("SET:a:1");
    leader.appendLogEntry("SET:b:2");
    leader.appendLogEntry("SET:c:3");
    
    // 假设 Follower 之前同步过部分日志，但重启后只剩1个
    follower.startElection();
    follower.becomeLeader();
    follower.appendLogEntry("SET:a:1");  // 只有第一个日志
    follower.becomeFollower(leader.getCurrentTerm());
    
    printNodeState("Leader 初始状态 (有3个日志)", leader, 0);
    printNodeState("Follower 初始状态 (只有1个日志)", follower);
    
    // 模拟：Leader 的 nextIndex[0] 被错误地设为很大的值
    // （真实场景：Follower 重启前 Leader 以为它有3个日志，nextIndex[0]=4）
    std::cout << "\n  💡 模拟场景：Leader 认为 Follower 有所有日志 (nextIndex[0]=4)" << std::endl;
    std::cout << "     （真实情况：Follower 重启丢失了日志2和3）" << std::endl;
    
    // 为了模拟这个场景，我们需要 Leader 重新成为 Leader
    // 这样 nextIndex 会被设为 lastLogIndex + 1 = 4
    int term = leader.getCurrentTerm();
    leader.becomeFollower(term + 1);
    leader.startElection();
    leader.becomeLeader();
    
    // === 第一次复制尝试（失败）===
    std::cout << "\n【尝试1】Leader 发送 AppendEntries（预期失败）" << std::endl;
    AppendEntriesArgs args1 = leader.createAppendEntriesArgs(0);
    printAppendEntriesArgs(args1);
    std::cout << "  💡 nextIndex[0]=4，所以 prevLogIndex=3，但 Follower 只有1个日志！" << std::endl;
    
    AppendEntriesReply reply1;
    follower.handleAppendEntries(args1, reply1);
    std::cout << "\n  📥 Reply: success=" << (reply1.success ? "true" : "false") << std::endl;
    std::cout << "     conflictIndex=" << reply1.conflictIndex 
              << ", conflictTerm=" << reply1.conflictTerm << std::endl;
    std::cout << "  ❌ 失败原因：Follower 的日志太短 (prevLogIndex=3 超出范围)" << std::endl;
    
    // === Leader 处理失败响应（回退 nextIndex）===
    std::cout << "\n【响应处理】Leader 回退 nextIndex" << std::endl;
    leader.handleAppendEntriesResponse(2, reply1);
    std::cout << "  🔄 nextIndex[0] 回退为: " << reply1.conflictIndex 
              << " (Follower 的日志长度)" << std::endl;
    
    // === 第二次复制尝试（成功）===
    std::cout << "\n【尝试2】Leader 重新发送 AppendEntries（预期成功）" << std::endl;
    AppendEntriesArgs args2 = leader.createAppendEntriesArgs(0);
    printAppendEntriesArgs(args2);
    std::cout << "  💡 nextIndex[0]=" << reply1.conflictIndex 
              << "，prevLogIndex=" << (reply1.conflictIndex - 1) << std::endl;
    
    AppendEntriesReply reply2;
    follower.handleAppendEntries(args2, reply2);
    std::cout << "\n  📥 Reply: success=" << (reply2.success ? "true" : "false") << std::endl;
    std::cout << "  ✅ 成功！Follower 日志与 Leader 同步" << std::endl;
    
    leader.handleAppendEntriesResponse(2, reply2);
    std::cout << "  🔄 nextIndex[0] 更新为: 4" << std::endl;
    
    printNodeState("最终 Leader 状态", leader, 0);
    printNodeState("最终 Follower 状态", follower);
    
    std::cout << "\n【总结】" << std::endl;
    std::cout << "  ✅ 失败时 nextIndex 自动回退到冲突位置" << std::endl;
    std::cout << "  ✅ 重试时从更早的位置开始发送日志" << std::endl;
    std::cout << "  ✅ 最终收敛到一致状态" << std::endl;
    std::cout << "  💡 这个过程完全自动，不需要手动干预" << std::endl;
}

int main() {
    std::cout << "\n";
    std::cout << "████████████████████████████████████████████████████████████\n";
    std::cout << "█                                                          █\n";
    std::cout << "█     Raft nextIndex 更新机制演示                          █\n";
    std::cout << "█                                                          █\n";
    std::cout << "████████████████████████████████████████████████████████████\n";
    
    try {
        demoNormalReplication();
        demoConflictResolution();
        
        std::cout << "\n\n";
        std::cout << "════════════════════════════════════════════════════════════\n";
        std::cout << " 核心结论：nextIndex 通过响应处理动态更新\n";
        std::cout << "════════════════════════════════════════════════════════════\n";
        std::cout << "\n1. 成功时：nextIndex[i] = lastLogIndex + 1\n";
        std::cout << "   → 下次从最新位置继续\n";
        std::cout << "\n2. 失败时：nextIndex[i] = conflictIndex (回退)\n";
        std::cout << "   → 下次从更早位置重试\n";
        std::cout << "\n3. 追加日志：不更新 nextIndex\n";
        std::cout << "   → 下次 RPC 自动包含新日志\n";
        std::cout << "\n4. 形成反馈循环：发送 → 响应 → 更新 → 发送...\n";
        std::cout << "   → 自适应地追上 Leader 的进度\n";
        std::cout << "\n════════════════════════════════════════════════════════════\n\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
