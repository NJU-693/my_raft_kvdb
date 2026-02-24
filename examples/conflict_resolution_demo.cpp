/**
 * @file conflict_resolution_demo.cpp
 * @brief 演示 Raft 日志冲突解决的多轮迭代过程
 * 
 * 回答问题：如果回退到 conflictIndex，但下次仍然不匹配怎么办？
 * 答案：会继续回退，直到找到匹配点！
 */

#include "raft/raft_node.h"
#include <iostream>
#include <iomanip>

using namespace raft;

// 打印日志状态
void printLog(const std::string& label, const RaftNode& node) {
    std::cout << label << ": ";
    int size = node.getLogSize();
    std::cout << "[";
    for (int i = 0; i <= size; ++i) {
        if (i > 0) std::cout << ", ";
        LogEntry entry = node.getLogEntry(i);
        std::cout << i << "(t=" << entry.term;
        if (!entry.command.empty()) {
            std::cout << ",'" << entry.command.substr(0, 5) << "'";
        }
        std::cout << ")";
    }
    std::cout << "]" << std::endl;
}

// 打印分隔线
void printSeparator(const std::string& title = "") {
    std::cout << "\n";
    if (title.empty()) {
        std::cout << std::string(70, '-') << std::endl;
    } else {
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "  " << title << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    }
}

/**
 * @brief 场景1：简单的多轮回退
 */
void scenario1_SimpleMultiRoundBackoff() {
    printSeparator("场景1：简单的多轮回退");
    
    std::cout << R"(
📖 场景描述：
  - Leader 有 5 个日志条目
  - Follower 有 3 个日志条目，但 index=2 和 3 的任期不匹配
  - 需要多轮回退才能找到匹配点

🎯 学习目标：
  理解冲突解决是一个迭代过程！
)";
    
    std::vector<int> peers1 = {2};
    std::vector<int> peers2 = {1};
    
    RaftNode leader(1, peers1);
    RaftNode follower(2, peers2);
    
    // 构造 Leader 的日志
    leader.startElection();
    leader.becomeLeader();
    leader.appendLogEntry("A");  // index=1, term=1
    leader.appendLogEntry("B");  // index=2, term=1
    leader.appendLogEntry("C");  // index=3, term=1
    leader.appendLogEntry("D");  // index=4, term=1
    
    // 重新成为 Leader 以更新 nextIndex
    int term = leader.getCurrentTerm();
    leader.becomeFollower(term + 1);
    leader.startElection();
    leader.becomeLeader();
    
    // 构造 Follower 的日志（有冲突）
    follower.startElection();
    follower.becomeLeader();
    follower.appendLogEntry("A");  // index=1, term=1 (相同)
    
    // 让 follower 进入更高的任期并追加不同的日志
    follower.becomeFollower(3);
    follower.startElection();
    follower.becomeLeader();
    follower.appendLogEntry("X");  // index=2, term=3 (不同！)
    follower.appendLogEntry("Y");  // index=3, term=3 (不同！)
    follower.becomeFollower(leader.getCurrentTerm());
    
    std::cout << "\n📊 初始状态：\n";
    printLog("Leader ", leader);
    printLog("Follower", follower);
    std::cout << "\n💡 注意：Leader 的 index=2,3,4 是 term=1\n";
    std::cout << "        Follower 的 index=2,3 是 term=3（冲突！）\n";
    
    // ===== 第一轮尝试 =====
    printSeparator("第 1 轮：尝试从 nextIndex=5 开始");
    
    std::cout << "\n📤 Leader 发送 AppendEntries:\n";
    AppendEntriesArgs args1 = leader.createAppendEntriesArgs(0);
    std::cout << "  prevLogIndex = " << args1.prevLogIndex << std::endl;
    std::cout << "  prevLogTerm = " << args1.prevLogTerm << std::endl;
    std::cout << "  entries.size() = " << args1.entries.size() << std::endl;
    
    std::cout << "\n📥 Follower 处理:\n";
    AppendEntriesReply reply1;
    follower.handleAppendEntries(args1, reply1);
    std::cout << "  ❌ 失败！log_[" << args1.prevLogIndex << "] 不存在（日志太短）\n";
    std::cout << "  conflictIndex = " << reply1.conflictIndex << " (Follower 的日志长度)\n";
    std::cout << "  conflictTerm = " << reply1.conflictTerm << std::endl;
    
    std::cout << "\n🔄 Leader 处理响应:\n";
    leader.handleAppendEntriesResponse(2, reply1);
    std::cout << "  nextIndex 回退到: " << reply1.conflictIndex << std::endl;
    
    // ===== 第二轮尝试 =====
    printSeparator("第 2 轮：尝试从 nextIndex=" + std::to_string(reply1.conflictIndex));
    
    std::cout << "\n📤 Leader 发送 AppendEntries:\n";
    AppendEntriesArgs args2 = leader.createAppendEntriesArgs(0);
    std::cout << "  prevLogIndex = " << args2.prevLogIndex << std::endl;
    std::cout << "  prevLogTerm = " << args2.prevLogTerm << std::endl;
    std::cout << "  entries.size() = " << args2.entries.size() << std::endl;
    
    std::cout << "\n📥 Follower 处理:\n";
    std::cout << "  检查：log_[" << args2.prevLogIndex << "].term == " << args2.prevLogTerm << " ?\n";
    LogEntry checkEntry = follower.getLogEntry(args2.prevLogIndex);
    std::cout << "  实际：log_[" << args2.prevLogIndex << "].term = " << checkEntry.term << std::endl;
    
    AppendEntriesReply reply2;
    follower.handleAppendEntries(args2, reply2);
    
    if (!reply2.success) {
        std::cout << "  ❌ 仍然失败！任期不匹配\n";
        std::cout << "  conflictTerm = " << reply2.conflictTerm 
                  << " (Follower 在 prevLogIndex 处的任期)\n";
        std::cout << "  conflictIndex = " << reply2.conflictIndex 
                  << " (任期 " << reply2.conflictTerm << " 的第一个位置)\n";
        
        std::cout << "\n💡 Follower 向前查找 term=" << reply2.conflictTerm << " 的第一个日志:\n";
        for (int i = args2.prevLogIndex; i >= 1; --i) {
            LogEntry e = follower.getLogEntry(i);
            std::cout << "    log_[" << i << "].term = " << e.term;
            if (e.term == reply2.conflictTerm) {
                std::cout << " ← 仍是 " << reply2.conflictTerm;
            } else {
                std::cout << " ← 不同了，停止";
                break;
            }
            std::cout << std::endl;
        }
        
        std::cout << "\n🔄 Leader 处理响应:\n";
        std::cout << "  Leader 有 term=" << reply2.conflictTerm << " 的日志吗？\n";
        std::cout << "  (检查 Leader 的日志...)\n";
        bool hasConflictTerm = false;
        for (int i = 1; i <= leader.getLogSize(); ++i) {
            if (leader.getLogEntry(i).term == reply2.conflictTerm) {
                hasConflictTerm = true;
                break;
            }
        }
        std::cout << "  答案：" << (hasConflictTerm ? "有" : "没有") << std::endl;
        
        leader.handleAppendEntriesResponse(2, reply2);
        std::cout << "  nextIndex 回退到: " << reply2.conflictIndex << std::endl;
    }
    
    // ===== 第三轮尝试 =====
    printSeparator("第 3 轮：尝试从 nextIndex=" + std::to_string(reply2.conflictIndex));
    
    std::cout << "\n📤 Leader 发送 AppendEntries:\n";
    AppendEntriesArgs args3 = leader.createAppendEntriesArgs(0);
    std::cout << "  prevLogIndex = " << args3.prevLogIndex << std::endl;
    std::cout << "  prevLogTerm = " << args3.prevLogTerm << std::endl;
    std::cout << "  entries.size() = " << args3.entries.size() << std::endl;
    
    std::cout << "\n📥 Follower 处理:\n";
    std::cout << "  检查：log_[" << args3.prevLogIndex << "].term == " << args3.prevLogTerm << " ?\n";
    LogEntry checkEntry3 = follower.getLogEntry(args3.prevLogIndex);
    std::cout << "  实际：log_[" << args3.prevLogIndex << "].term = " << checkEntry3.term << std::endl;
    
    AppendEntriesReply reply3;
    follower.handleAppendEntries(args3, reply3);
    
    if (reply3.success) {
        std::cout << "  ✅ 成功匹配！\n";
        std::cout << "  Follower 删除 log_[" << (args3.prevLogIndex + 1) << "] 及之后的日志\n";
        std::cout << "  Follower 追加 Leader 的 " << args3.entries.size() << " 个新日志条目\n";
        
        leader.handleAppendEntriesResponse(2, reply3);
        
        std::cout << "\n📊 最终状态：\n";
        printLog("Leader ", leader);
        printLog("Follower", follower);
        std::cout << "\n🎉 日志同步完成！\n";
    }
    
    printSeparator();
    std::cout << R"(
📚 总结：
  1️⃣  第1轮：nextIndex=5，prevLogIndex=4 → Follower 日志太短 → 回退到 4
  2️⃣  第2轮：nextIndex=4，prevLogIndex=3 → 任期不匹配(3≠1) → 回退到 2
  3️⃣  第3轮：nextIndex=2，prevLogIndex=1 → 任期匹配(1==1) → 成功！

💡 关键理解：
  - 冲突解决是一个迭代过程
  - 每次失败都会回退一段距离
  - 最终会找到匹配点（最坏回退到索引0）
  - 这不是 bug，而是设计！

🔑 回答你的问题：
  "如果回退到 conflictIndex，但下次仍然不匹配怎么办？"
  
  答案：会再次触发冲突检测，继续回退，直到成功！
  就像本例，经过了 3 轮才找到匹配点。
)";
}

/**
 * @brief 场景2：最坏情况 - 回退到起点
 */
void scenario2_WorstCase() {
    printSeparator("场景2：最坏情况 - 需要回退到起点");
    
    std::cout << R"(
📖 场景描述：
  - Leader 和 Follower 的日志从 index=1 开始就完全不同
  - 需要回退到 index=0（哨兵）才能匹配

🎯 学习目标：
  理解最坏情况下的收敛性保证
)";
    
    std::vector<int> peers1 = {2};
    std::vector<int> peers2 = {1};
    
    RaftNode leader(1, peers1);
    RaftNode follower(2, peers2);
    
    // Leader 的日志：全是 term=1
    leader.startElection();
    leader.becomeLeader();
    leader.appendLogEntry("L1");
    leader.appendLogEntry("L2");
    leader.appendLogEntry("L3");
    
    // Follower 的日志：不同的任期
    follower.startElection();
    follower.becomeLeader();
    follower.appendLogEntry("F1");  // term=1
    
    follower.becomeFollower(2);
    follower.startElection();
    follower.becomeLeader();
    follower.appendLogEntry("F2");  // term=2
    
    follower.becomeFollower(3);
    follower.startElection();
    follower.becomeLeader();
    follower.appendLogEntry("F3");  // term=3
    
    // 让 leader 重新成为 leader
    int term = leader.getCurrentTerm();
    leader.becomeFollower(4);
    leader.startElection();
    leader.becomeLeader();
    
    follower.becomeFollower(leader.getCurrentTerm());
    
    std::cout << "\n📊 初始状态（极端不匹配）：\n";
    printLog("Leader ", leader);
    printLog("Follower", follower);
    
    int round = 1;
    bool success = false;
    
    while (!success && round <= 5) {
        printSeparator("第 " + std::to_string(round) + " 轮");
        
        AppendEntriesArgs args = leader.createAppendEntriesArgs(0);
        std::cout << "\n📤 prevLogIndex=" << args.prevLogIndex 
                  << ", prevLogTerm=" << args.prevLogTerm << std::endl;
        
        AppendEntriesReply reply;
        follower.handleAppendEntries(args, reply);
        
        if (reply.success) {
            std::cout << "✅ 成功！\n";
            success = true;
        } else {
            std::cout << "❌ 失败 → conflictIndex=" << reply.conflictIndex 
                      << ", conflictTerm=" << reply.conflictTerm << std::endl;
            leader.handleAppendEntriesResponse(2, reply);
        }
        
        round++;
    }
    
    std::cout << "\n📊 最终状态：\n";
    printLog("Leader ", leader);
    printLog("Follower", follower);
    
    printSeparator();
    std::cout << R"(
📚 总结：
  即使完全不匹配，最多经过 O(log_size) 轮也能收敛到 index=0（哨兵节点）
  index=0 的 term=0 总是匹配的，然后就可以从头开始同步整个日志。

🛡️  收敛性保证：
  - nextIndex 单调递减（除非成功）
  - 最小值是 1（代码保证）
  - prevLogIndex = nextIndex - 1，最小是 0
  - log_[0] 是哨兵节点，term=0，所有节点都相同
  - 因此必然收敛！
)";
}

int main() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════════════╗
║                                                                      ║
║         Raft 日志冲突解决：多轮迭代过程演示                          ║
║                                                                      ║
║  问题：回退到 conflictIndex 后，如果仍然不匹配怎么办？               ║
║  答案：继续回退，直到找到匹配点！                                    ║
║                                                                      ║
╚══════════════════════════════════════════════════════════════════════╝
)";
    
    try {
        scenario1_SimpleMultiRoundBackoff();
        scenario2_WorstCase();
        
        std::cout << "\n\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                          核心结论                                    ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
        std::cout << R"(
1. 冲突解决是迭代的，不是一次完成的
   ❌ 错误理解：一次回退就要找到正确位置
   ✅ 正确理解：多次尝试，逐步回退，直到成功

2. 每次失败都提供新的冲突信息
   - conflictTerm：Follower 在冲突位置的任期
   - conflictIndex：该任期的第一个位置
   → Leader 根据这些信息决定回退多远

3. 收敛性保证
   - nextIndex 单调递减（每次失败都回退）
   - 有下界（最小是 1）
   - 哨兵节点（index=0）总是匹配
   → 必然在有限轮次内收敛

4. 优化效果
   - 不优化：每次回退 1 个位置，可能需要 N 轮
   - 优化后：跳过整个冲突任期，通常 2-3 轮
   - 网络分区恢复后，通常 3-5 轮 RPC 完成同步

5. 这是正常的，不是 bug！
   网络分区、节点重启等场景都会导致多轮回退
   系统设计就是为了处理这些情况
)" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Demo failed: " << e.what() << std::endl;
        return 1;
    }
}
