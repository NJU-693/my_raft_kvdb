#ifndef RAFT_RAFT_NODE_H
#define RAFT_RAFT_NODE_H

#include "log_entry.h"
#include "rpc_messages.h"
#include <vector>
#include <mutex>
#include <chrono>
#include <random>
#include <atomic>
#include <set>
#include <functional>

namespace raft {

/**
 * @brief Raft 节点状态枚举
 * 
 * 需求: 2.1.1 (领导者选举)
 */
enum class NodeState {
    FOLLOWER,   // 跟随者：被动接收 Leader 的日志和心跳
    CANDIDATE,  // 候选人：发起选举，请求其他节点投票
    LEADER      // 领导者：处理客户端请求，复制日志到 Follower
};

/**
 * @brief Raft 节点类
 * 
 * 实现 Raft 共识算法的核心逻辑，包括：
 * - 领导者选举
 * - 日志复制
 * - 状态管理
 * - 定时器机制
 * 
 * 需求: 2.1.1 (领导者选举), 2.1.2 (日志复制)
 */
class RaftNode {
public:
    /**
     * @brief 构造函数
     * @param nodeId 节点 ID
     * @param peerIds 集群中其他节点的 ID 列表
     */
    RaftNode(int nodeId, const std::vector<int>& peerIds);

    /**
     * @brief 析构函数
     */
    ~RaftNode();

    // ==================== 状态查询接口 ====================

    /**
     * @brief 获取当前节点状态
     */
    NodeState getState() const;

    /**
     * @brief 获取当前任期
     */
    int getCurrentTerm() const;

    /**
     * @brief 获取当前 Leader ID
     */
    int getLeaderId() const;

    /**
     * @brief 判断当前节点是否为 Leader
     */
    bool isLeader() const;

    // ==================== RPC 处理接口 ====================

    /**
     * @brief 处理 RequestVote RPC 请求
     * @param args 请求参数
     * @param reply 响应结果
     * 
     * 需求: 2.1.1 (领导者选举)
     */
    void handleRequestVote(const RequestVoteArgs& args, RequestVoteReply& reply);

    /**
     * @brief 处理 AppendEntries RPC 请求
     * @param args 请求参数
     * @param reply 响应结果
     * 
     * 需求: 2.1.2 (日志复制)
     */
    void handleAppendEntries(const AppendEntriesArgs& args, AppendEntriesReply& reply);

    // ==================== 状态转换接口 ====================

    /**
     * @brief 发起选举
     * 
     * Follower 或 Candidate 在选举超时后调用此方法。
     * 转变为 Candidate，增加任期，投票给自己，并向其他节点请求投票。
     * 
     * 需求: 2.1.1 (领导者选举)
     */
    void startElection();

    /**
     * @brief 处理投票响应
     * @param nodeId 响应节点的 ID
     * @param reply 投票响应
     * @return 如果获得多数票成为 Leader 返回 true
     * 
     * Candidate 收到投票响应后调用此方法。
     * 统计投票结果，如果获得多数票则成为 Leader。
     * 
     * 需求: 2.1.1 (领导者选举)
     */
    bool handleVoteResponse(int nodeId, const RequestVoteReply& reply);

    /**
     * @brief 成为 Leader
     * 
     * Candidate 获得多数票后调用此方法。
     * 转变为 Leader，初始化 nextIndex 和 matchIndex，开始发送心跳。
     * 
     * 需求: 2.1.1 (领导者选举)
     */
    void becomeLeader();

    /**
     * @brief 成为 Follower
     * @param term 新的任期号
     * 
     * 当发现更高任期或收到有效的 Leader 消息时调用。
     * 
     * 需求: 2.1.1 (领导者选举)
     */
    void becomeFollower(int term);

    // ==================== 定时器接口 ====================

    /**
     * @brief 检查选举超时
     * @return 如果选举超时返回 true
     * 
     * 需求: 2.1.1 (领导者选举)
     */
    bool checkElectionTimeout() const;

    /**
     * @brief 重置选举超时计时器
     * 
     * 在收到有效的 Leader 消息或开始新选举时调用。
     * 
     * 需求: 2.1.1 (领导者选举)
     */
    void resetElectionTimer();

    /**
     * @brief 检查是否需要发送心跳
     * @return 如果需要发送心跳返回 true
     * 
     * 需求: 2.1.2 (日志复制)
     */
    bool shouldSendHeartbeat() const;

    /**
     * @brief 重置心跳计时器
     * 
     * 在发送心跳后调用。
     * 
     * 需求: 2.1.2 (日志复制)
     */
    void resetHeartbeatTimer();

    // ==================== 日志操作接口 ====================

    /**
     * @brief 追加日志条目
     * @param command 命令字符串
     * @return 日志索引
     * 
     * Leader 接收客户端请求后调用此方法。
     * 
     * 需求: 2.1.2 (日志复制)
     */
    int appendLogEntry(const std::string& command);

    /**
     * @brief 获取日志条目
     * @param index 日志索引
     * @return 日志条目（如果不存在返回空的 LogEntry）
     */
    LogEntry getLogEntry(int index) const;

    /**
     * @brief 获取最后一个日志条目的索引
     */
    int getLastLogIndex() const;

    /**
     * @brief 获取最后一个日志条目的任期
     */
    int getLastLogTerm() const;

    /**
     * @brief 获取日志大小
     */
    int getLogSize() const;

    /**
     * @brief 获取当前收到的投票数
     * @return 当前选举中收到的投票数（仅在 Candidate 状态有效）
     */
    int getVoteCount() const;

    /**
     * @brief 计算多数票阈值
     * @return 获得领导权需要的最少票数
     */
    int getMajorityThreshold() const;

    // ==================== 日志复制接口 ====================

    /**
     * @brief 复制日志到所有 Follower
     * 
     * Leader 调用此方法向所有 Follower 发送 AppendEntries RPC。
     * 包括心跳和实际的日志条目复制。
     * 
     * 需求: 2.1.2 (日志复制)
     */
    void replicateLog();

    /**
     * @brief 处理 AppendEntries 响应
     * @param nodeId 响应节点的 ID
     * @param reply AppendEntries 响应
     * @return 如果需要更新 commitIndex 返回 true
     * 
     * Leader 收到 Follower 的 AppendEntries 响应后调用此方法。
     * 更新 nextIndex 和 matchIndex，处理日志冲突。
     * 
     * 需求: 2.1.2 (日志复制)
     */
    bool handleAppendEntriesResponse(int nodeId, const AppendEntriesReply& reply);

    /**
     * @brief 应用已提交的日志条目到状态机
     * 
     * 将 lastApplied 到 commitIndex 之间的日志条目应用到状态机。
     * 所有节点都需要调用此方法。
     * 
     * 需求: 2.1.2 (日志复制)
     */
    void applyCommittedEntries();

    /**
     * @brief 更新提交索引
     * 
     * Leader 检查是否有新的日志条目可以提交。
     * 当多数节点复制了某个日志条目时，该条目可以被提交。
     * 
     * 需求: 2.1.2 (日志复制)
     */
    void updateCommitIndex();

    /**
     * @brief 为指定节点创建 AppendEntries 参数
     * @param nodeIndex 节点在 peerIds_ 中的索引
     * @return AppendEntries 参数
     * 
     * 根据节点的 nextIndex 创建相应的 AppendEntries 参数。
     * 
     * 需求: 2.1.2 (日志复制)
     */
    AppendEntriesArgs createAppendEntriesArgs(int nodeIndex) const;

    /**
     * @brief 获取提交索引
     */
    int getCommitIndex() const;

    /**
     * @brief 获取已应用索引
     */
    int getLastApplied() const;

    /**
     * @brief 设置状态机应用回调
     * @param callback 状态机应用回调函数
     * 
     * 当日志条目被应用到状态机时调用此回调。
     */
    void setApplyCallback(std::function<void(const std::string&)> callback);

private:
    // ==================== 持久化状态（所有服务器）====================
    // 需求: 2.1.3 (安全性保证 - 持久化存储)
    
    int currentTerm_;              // 当前任期号（初始为 0，单调递增）
    int votedFor_;                 // 当前任期投票给了谁（-1 表示未投票）
    std::vector<LogEntry> log_;    // 日志条目数组（索引从 1 开始，0 位置为哨兵）

    // ==================== 易失状态（所有服务器）====================
    
    int commitIndex_;              // 已知已提交的最高日志索引（初始为 0，单调递增）
    int lastApplied_;              // 已应用到状态机的最高日志索引（初始为 0，单调递增）

    // ==================== 易失状态（Leader）====================
    
    std::vector<int> nextIndex_;   // 对于每个服务器，下一个要发送的日志索引（初始为 Leader 最后日志索引 + 1）
    std::vector<int> matchIndex_;  // 对于每个服务器，已知已复制的最高日志索引（初始为 0，单调递增）

    // ==================== 节点状态 ====================
    
    int nodeId_;                   // 当前节点 ID
    std::vector<int> peerIds_;     // 集群中其他节点的 ID 列表
    NodeState state_;              // 当前节点状态
    int leaderId_;                 // 当前 Leader ID（-1 表示未知）

    // ==================== 选举状态 ====================
    
    std::set<int> votesReceived_;  // 当前任期收到的投票节点 ID 集合
    int currentElectionTerm_;      // 当前选举的任期号（用于防止重复计票）

    // ==================== 定时器相关 ====================
    
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::milliseconds;

    TimePoint lastHeartbeatTime_;  // 上次收到心跳的时间（Follower/Candidate）
    TimePoint lastHeartbeatSent_;  // 上次发送心跳的时间（Leader）
    Duration electionTimeout_;     // 选举超时时间（随机化）
    Duration heartbeatInterval_;   // 心跳间隔（固定）

    // 选举超时范围（毫秒）
    static constexpr int ELECTION_TIMEOUT_MIN = 150;
    static constexpr int ELECTION_TIMEOUT_MAX = 300;
    static constexpr int HEARTBEAT_INTERVAL = 50;

    // ==================== 线程安全 ====================
    
    mutable std::mutex mutex_;     // 保护所有状态的互斥锁

    // ==================== 随机数生成器 ====================
    
    std::mt19937 rng_;             // 随机数生成器（用于选举超时）

    // ==================== 状态机应用回调 ====================
    
    std::function<void(const std::string&)> applyCallback_;  // 状态机应用回调

    // ==================== 私有辅助方法 ====================

    /**
     * @brief 生成随机的选举超时时间
     */
    Duration generateElectionTimeout();

    /**
     * @brief 检查候选人的日志是否至少和自己一样新
     * @param lastLogIndex 候选人最后日志索引
     * @param lastLogTerm 候选人最后日志任期
     * @return 如果候选人日志至少和自己一样新返回 true
     * 
     * 需求: 2.1.3 (安全性保证 - 选举限制)
     */
    bool isLogUpToDate(int lastLogIndex, int lastLogTerm) const;

    /**
     * @brief 成为 Leader（内部方法，假设调用者已持有锁）
     */
    void becomeLeaderInternal();

    /**
     * @brief 应用已提交的日志条目到状态机（内部方法，假设调用者已持有锁）
     */
    void applyCommittedEntriesInternal();
};

} // namespace raft

#endif // RAFT_RAFT_NODE_H