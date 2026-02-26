#include "raft/raft_node.h"
#include "network/rpc_client.h"
#include <algorithm>
#include <condition_variable>
#include <thread>
#include <iostream>

namespace raft {

RaftNode::RaftNode(int nodeId, const std::vector<int>& peerIds)
    : currentTerm_(0),
      votedFor_(-1),
      commitIndex_(0),
      lastApplied_(0),
      nodeId_(nodeId),
      peerIds_(peerIds),
      state_(NodeState::FOLLOWER),
      leaderId_(-1),
      currentElectionTerm_(-1),
      lastHeartbeatTime_(std::chrono::steady_clock::now()),
      lastHeartbeatSent_(std::chrono::steady_clock::now()),
      heartbeatInterval_(HEARTBEAT_INTERVAL),
      rng_(std::random_device{}()) {
    
    // 初始化日志（索引 0 为哨兵）
    log_.push_back(LogEntry(0, "", 0));
    
    // 初始化 Leader 状态（虽然初始为 Follower，但预先分配空间）
    nextIndex_.resize(peerIds_.size(), 1);
    matchIndex_.resize(peerIds_.size(), 0);
    
    // 生成随机选举超时时间
    electionTimeout_ = generateElectionTimeout();
}

RaftNode::~RaftNode() {
    // 清理资源
}

// ==================== 状态查询接口 ====================

NodeState RaftNode::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

int RaftNode::getCurrentTerm() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentTerm_;
}

int RaftNode::getLeaderId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return leaderId_;
}

bool RaftNode::isLeader() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == NodeState::LEADER;
}

// ==================== RPC 处理接口 ====================

void RaftNode::handleRequestVote(const RequestVoteArgs& args, RequestVoteReply& reply) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 初始化响应
    reply.term = currentTerm_;
    reply.voteGranted = false;
    
    // 规则 1: 如果 term < currentTerm，拒绝投票
    if (args.term < currentTerm_) {
        return;
    }
    
    // 规则 2: 如果 term > currentTerm，更新任期并转为 Follower
    if (args.term > currentTerm_) {
        currentTerm_ = args.term;
        votedFor_ = -1;
        state_ = NodeState::FOLLOWER;
        leaderId_ = -1;
        votesReceived_.clear();
        currentElectionTerm_ = -1;
        reply.term = currentTerm_;
    }
    
    // 规则 3: 检查投票条件
    // - 当前任期还未投票，或已经投给了该候选人
    // - 候选人的日志至少和自己一样新
    bool canVote = (votedFor_ == -1 || votedFor_ == args.candidateId);
    bool logUpToDate = isLogUpToDate(args.lastLogIndex, args.lastLogTerm);
    
    if (canVote && logUpToDate) {
        votedFor_ = args.candidateId;
        reply.voteGranted = true;
        resetElectionTimer();  // 重置选举超时
    }
}

void RaftNode::handleAppendEntries(const AppendEntriesArgs& args, AppendEntriesReply& reply) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 初始化响应
    reply.term = currentTerm_;
    reply.success = false;
    reply.conflictIndex = -1;
    reply.conflictTerm = -1;
    
    // 规则 1: 如果 term < currentTerm，拒绝请求
    if (args.term < currentTerm_) {
        return;
    }
    
    // 规则 2: 如果 term >= currentTerm，更新任期并转为 Follower
    if (args.term >= currentTerm_) {
        currentTerm_ = args.term;
        if (state_ != NodeState::FOLLOWER) {
            state_ = NodeState::FOLLOWER;
            votedFor_ = -1;
            votesReceived_.clear();
            currentElectionTerm_ = -1;
        }
        leaderId_ = args.leaderId;
        resetElectionTimer();  // 重置选举超时
        reply.term = currentTerm_;
    }
    
    // 规则 3: 检查日志一致性
    // 如果 prevLogIndex 处的日志不存在或任期不匹配，返回失败
    if (args.prevLogIndex > 0) {
        if (args.prevLogIndex >= static_cast<int>(log_.size())) {
            // 日志不够长
            reply.conflictIndex = log_.size();
            reply.conflictTerm = -1;
            return;
        }
        
        if (log_[args.prevLogIndex].term != args.prevLogTerm) {
            // 任期不匹配
            reply.conflictTerm = log_[args.prevLogIndex].term;
            // 找到该任期的第一个日志索引
            reply.conflictIndex = args.prevLogIndex;
            while (reply.conflictIndex > 1 && 
                   log_[reply.conflictIndex - 1].term == reply.conflictTerm) {
                reply.conflictIndex--;
            }
            return;
        }
    }
    
    // 规则 4: 如果存在冲突的日志条目，删除它及其后的所有条目
    int logIndex = args.prevLogIndex + 1;
    for (size_t i = 0; i < args.entries.size(); ++i) {
        int currentIndex = logIndex + i;
        
        if (currentIndex < static_cast<int>(log_.size())) {
            // 检查是否冲突
            if (log_[currentIndex].term != args.entries[i].term) {
                // 删除冲突的条目及其后的所有条目
                log_.erase(log_.begin() + currentIndex, log_.end());
                // 追加新条目
                LogEntry newEntry = args.entries[i];
                newEntry.index = currentIndex;  // 确保索引正确
                log_.push_back(newEntry);
            }
            // 如果不冲突，跳过（已存在相同的条目）
        } else {
            // 追加新条目
            LogEntry newEntry = args.entries[i];
            newEntry.index = currentIndex;  // 确保索引正确
            log_.push_back(newEntry);
        }
    }
    
    // 规则 5: 更新 commitIndex
    if (args.leaderCommit > commitIndex_) {
        commitIndex_ = std::min(args.leaderCommit, getLastLogIndex());
        // 应用新提交的日志条目
        applyCommittedEntriesInternal();
    }
    
    reply.success = true;
}

// ==================== 状态转换接口 ====================

void RaftNode::startElection() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 转变为 Candidate
    state_ = NodeState::CANDIDATE;
    currentTerm_++;
    votedFor_ = nodeId_;  // 投票给自己
    leaderId_ = -1;
    
    // 初始化选举状态
    currentElectionTerm_ = currentTerm_;
    votesReceived_.clear();
    votesReceived_.insert(nodeId_);  // 自己的票
    
    // 重置选举超时
    resetElectionTimer();
    
    // 向所有对等节点发送 RequestVote RPC
    for (size_t i = 0; i < peerIds_.size(); ++i) {
        std::thread([this, i]() {
            sendRequestVoteAsync(peerIds_[i]);
        }).detach();
    }
}

bool RaftNode::handleVoteResponse(int nodeId, const RequestVoteReply& reply) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 只有 Candidate 才处理投票响应
    if (state_ != NodeState::CANDIDATE) {
        return false;
    }
    
    // 检查任期是否匹配当前选举
    if (reply.term > currentTerm_) {
        // 发现更高任期，转为 Follower
        currentTerm_ = reply.term;
        votedFor_ = -1;
        state_ = NodeState::FOLLOWER;
        leaderId_ = -1;
        votesReceived_.clear();
        resetElectionTimer();
        return false;
    }
    
    // 忽略过期的投票响应
    if (reply.term < currentElectionTerm_) {
        return false;
    }
    
    // 处理投票结果
    if (reply.voteGranted) {
        votesReceived_.insert(nodeId);
        
        // 检查是否获得多数票
        int totalNodes = peerIds_.size() + 1;  // 包括自己
        int majority = totalNodes / 2 + 1;
        
        if (static_cast<int>(votesReceived_.size()) >= majority) {
            // 获得多数票，成为 Leader
            becomeLeaderInternal();
            return true;
        }
    }
    
    return false;
}

void RaftNode::becomeLeader() {
    std::lock_guard<std::mutex> lock(mutex_);
    becomeLeaderInternal();
}

void RaftNode::becomeLeaderInternal() {
    // 注意：此方法假设调用者已持有锁
    
    // 转变为 Leader
    state_ = NodeState::LEADER;
    leaderId_ = nodeId_;
    
    // 清理选举状态
    votesReceived_.clear();
    currentElectionTerm_ = -1;
    
    // 初始化 nextIndex 和 matchIndex
    int lastLogIndex = getLastLogIndex();
    for (size_t i = 0; i < peerIds_.size(); ++i) {
        nextIndex_[i] = lastLogIndex + 1;
        matchIndex_[i] = 0;
    }
    
    // 重置心跳计时器
    resetHeartbeatTimer();
    
    // 注意：实际的心跳发送需要在外部实现（涉及网络通信）
}

void RaftNode::becomeFollower(int term) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 更新任期
    if (term > currentTerm_) {
        currentTerm_ = term;
        votedFor_ = -1;
    }
    
    // 转变为 Follower
    state_ = NodeState::FOLLOWER;
    leaderId_ = -1;
    
    // 清理选举状态
    votesReceived_.clear();
    currentElectionTerm_ = -1;
    
    // 重置选举超时
    resetElectionTimer();
}

// ==================== 定时器接口 ====================

bool RaftNode::checkElectionTimeout() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Leader 不需要检查选举超时
    if (state_ == NodeState::LEADER) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<Duration>(now - lastHeartbeatTime_);
    return elapsed >= electionTimeout_;
}

void RaftNode::resetElectionTimer() {
    // 注意：此方法假设调用者已持有锁
    lastHeartbeatTime_ = std::chrono::steady_clock::now();
    electionTimeout_ = generateElectionTimeout();
}

bool RaftNode::shouldSendHeartbeat() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 只有 Leader 需要发送心跳
    if (state_ != NodeState::LEADER) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<Duration>(now - lastHeartbeatSent_);
    return elapsed >= heartbeatInterval_;
}

void RaftNode::resetHeartbeatTimer() {
    // 注意：此方法假设调用者已持有锁
    lastHeartbeatSent_ = std::chrono::steady_clock::now();
}

// ==================== 日志操作接口 ====================

int RaftNode::appendLogEntry(const std::string& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 只有 Leader 可以追加日志
    if (state_ != NodeState::LEADER) {
        return -1;
    }
    
    // 创建新的日志条目
    int index = log_.size();
    LogEntry entry(currentTerm_, command, index);
    log_.push_back(entry);
    
    return index;
}

LogEntry RaftNode::getLogEntry(int index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (index < 0 || index >= static_cast<int>(log_.size())) {
        return LogEntry();  // 返回空的日志条目
    }
    
    return log_[index];
}

int RaftNode::getLastLogIndex() const {
    // 注意：此方法假设调用者已持有锁
    return log_.size() - 1;
}

int RaftNode::getLastLogTerm() const {
    // 注意：此方法假设调用者已持有锁
    if (log_.empty()) {
        return 0;
    }
    return log_.back().term;
}

int RaftNode::getLogSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return log_.size() - 1;  // 减去哨兵节点
}

int RaftNode::getVoteCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(votesReceived_.size());
}

int RaftNode::getMajorityThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int totalNodes = peerIds_.size() + 1;  // 包括自己
    return totalNodes / 2 + 1;
}

// ==================== 日志复制接口 ====================

void RaftNode::replicateLog() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 只有 Leader 可以复制日志
    if (state_ != NodeState::LEADER) {
        return;
    }
    
    // 向所有对等节点发送 AppendEntries RPC
    for (size_t i = 0; i < peerIds_.size(); ++i) {
        std::thread([this, i]() {
            sendAppendEntriesAsync(i);
        }).detach();
    }
}

bool RaftNode::handleAppendEntriesResponse(int nodeId, const AppendEntriesReply& reply) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "[handleAppendEntriesResponse] Received response from node " << nodeId 
              << ", success=" << reply.success << ", term=" << reply.term << std::endl;
    
    // 只有 Leader 处理 AppendEntries 响应
    if (state_ != NodeState::LEADER) {
        std::cout << "[handleAppendEntriesResponse] Not leader, ignoring response" << std::endl;
        return false;
    }
    
    // 找到节点在 peerIds_ 中的索引
    int nodeIndex = -1;
    for (size_t i = 0; i < peerIds_.size(); ++i) {
        if (peerIds_[i] == nodeId) {
            nodeIndex = i;
            break;
        }
    }
    
    if (nodeIndex == -1) {
        std::cout << "[handleAppendEntriesResponse] Unknown node " << nodeId << std::endl;
        return false;  // 未知节点
    }
    
    std::cout << "[handleAppendEntriesResponse] Node index: " << nodeIndex 
              << ", current matchIndex: " << matchIndex_[nodeIndex] 
              << ", nextIndex: " << nextIndex_[nodeIndex] << std::endl;
    
    // 检查任期
    if (reply.term > currentTerm_) {
        // 发现更高任期，转为 Follower
        std::cout << "[handleAppendEntriesResponse] Higher term detected, becoming follower" << std::endl;
        currentTerm_ = reply.term;
        votedFor_ = -1;
        state_ = NodeState::FOLLOWER;
        leaderId_ = -1;
        votesReceived_.clear();
        resetElectionTimer();
        return false;
    }
    
    if (reply.success) {
        // 成功复制日志
        // 更新 nextIndex 和 matchIndex
        // 注意：matchIndex 应该基于实际发送的日志，而不是当前的 lastLogIndex
        // 因为在发送 AppendEntries 后，Leader 可能又追加了新的日志条目
        int prevNextIndex = nextIndex_[nodeIndex];
        int lastLogIndex = getLastLogIndex();
        
        // matchIndex 应该是 Follower 已确认复制的最高索引
        // 即 prevNextIndex - 1 + 发送的条目数量
        // 但由于我们发送了从 prevNextIndex 到 lastLogIndex 的所有条目
        // 所以 matchIndex 应该是发送时的 lastLogIndex
        // 为了安全起见，我们使用 min(当前 lastLogIndex, 预期的 matchIndex)
        int expectedMatchIndex = lastLogIndex;
        matchIndex_[nodeIndex] = expectedMatchIndex;
        nextIndex_[nodeIndex] = expectedMatchIndex + 1;
        
        std::cout << "[handleAppendEntriesResponse] Updated matchIndex[" << nodeIndex << "] to " 
                  << matchIndex_[nodeIndex] << ", nextIndex to " << nextIndex_[nodeIndex] << std::endl;
        
        // 检查是否可以更新 commitIndex
        updateCommitIndex();
        return true;
    } else {
        // 日志复制失败，需要回退 nextIndex
        if (reply.conflictTerm == -1) {
            // Follower 的日志太短
            nextIndex_[nodeIndex] = reply.conflictIndex;
        } else {
            // 存在冲突的任期
            // 查找 Leader 日志中该任期的最后一个条目
            int conflictTermLastIndex = -1;
            for (int i = getLastLogIndex(); i >= 1; --i) {
                if (log_[i].term == reply.conflictTerm) {
                    conflictTermLastIndex = i;
                    break;
                }
            }
            
            if (conflictTermLastIndex != -1) {
                // Leader 也有该任期的日志，从该任期的最后一个条目之后开始
                nextIndex_[nodeIndex] = conflictTermLastIndex + 1;
            } else {
                // Leader 没有该任期的日志，从冲突索引开始
                nextIndex_[nodeIndex] = reply.conflictIndex;
            }
        }
        
        // 确保 nextIndex 不小于 1
        if (nextIndex_[nodeIndex] < 1) {
            nextIndex_[nodeIndex] = 1;
        }
        
        return false;
    }
}

void RaftNode::applyCommittedEntries() {
    std::lock_guard<std::mutex> lock(mutex_);
    applyCommittedEntriesInternal();
}

void RaftNode::applyCommittedEntriesInternal() {
    // 注意：此方法假设调用者已持有锁
    
    // 应用 lastApplied + 1 到 commitIndex 之间的所有日志条目
    while (lastApplied_ < commitIndex_) {
        lastApplied_++;
        
        std::cout << "[applyCommittedEntriesInternal] Applying log entry at index " << lastApplied_ 
                  << " (commitIndex: " << commitIndex_ << ")" << std::endl;
        
        if (lastApplied_ < static_cast<int>(log_.size())) {
            const LogEntry& entry = log_[lastApplied_];
            
            std::cout << "[applyCommittedEntriesInternal] Entry command: " << entry.command << std::endl;
            
            // 调用状态机应用回调
            std::string result = "OK";
            if (applyCallback_) {
                result = applyCallback_(entry.command);
                std::cout << "[applyCommittedEntriesInternal] Callback result: " << result << std::endl;
            } else {
                std::cout << "[applyCommittedEntriesInternal] WARNING: No applyCallback set!" << std::endl;
            }
            
            // 通知等待该命令的客户端
            std::shared_ptr<PendingCommand> pendingCmd;
            {
                std::lock_guard<std::mutex> lock(pendingCommandsMutex_);
                auto it = pendingCommands_.find(lastApplied_);
                if (it != pendingCommands_.end()) {
                    pendingCmd = it->second;
                    std::cout << "[applyCommittedEntriesInternal] Found pending command for index " << lastApplied_ << std::endl;
                } else {
                    std::cout << "[applyCommittedEntriesInternal] No pending command for index " << lastApplied_ 
                              << " (total pending: " << pendingCommands_.size() << ")" << std::endl;
                }
            }
            
            if (pendingCmd) {
                std::lock_guard<std::mutex> lock(pendingCmd->mutex);
                pendingCmd->result = result;
                pendingCmd->completed = true;
                pendingCmd->cv.notify_one();
                std::cout << "[applyCommittedEntriesInternal] Notified pending command at index " << lastApplied_ << std::endl;
            }
        } else {
            std::cout << "[applyCommittedEntriesInternal] WARNING: lastApplied_ " << lastApplied_ 
                      << " >= log size " << log_.size() << std::endl;
        }
    }
}

void RaftNode::updateCommitIndex() {
    // 注意：此方法假设调用者已持有锁
    
    // 只有 Leader 可以更新 commitIndex
    if (state_ != NodeState::LEADER) {
        return;
    }
    
    // 保存旧的 commitIndex 以便检测是否有更新
    int oldCommitIndex = commitIndex_;
    
    // 找到可以提交的最高日志索引
    // 需要多数节点都已复制的日志条目
    int lastLogIndex = getLastLogIndex();
    int totalNodes = peerIds_.size() + 1;  // 包括自己
    int majority = totalNodes / 2 + 1;
    
    std::cout << "[updateCommitIndex] Leader checking commit: lastLogIndex=" << lastLogIndex 
              << ", currentCommitIndex=" << commitIndex_ 
              << ", totalNodes=" << totalNodes 
              << ", majority=" << majority << std::endl;
    
    for (int index = lastLogIndex; index > commitIndex_; --index) {
        // 检查当前任期的日志条目（安全性要求）
        if (log_[index].term != currentTerm_) {
            std::cout << "[updateCommitIndex] Skipping index " << index 
                      << " (term " << log_[index].term << " != currentTerm " << currentTerm_ << ")" << std::endl;
            continue;
        }
        
        // 统计已复制该索引的节点数量（包括 Leader 自己）
        int replicatedCount = 1;  // Leader 自己已经有这个日志
        for (size_t i = 0; i < matchIndex_.size(); ++i) {
            if (matchIndex_[i] >= index) {
                replicatedCount++;
            }
        }
        
        std::cout << "[updateCommitIndex] Index " << index 
                  << " replicated on " << replicatedCount << " nodes (need " << majority << ")" << std::endl;
        
        // 如果多数节点已复制，则可以提交
        if (replicatedCount >= majority) {
            commitIndex_ = index;
            std::cout << "[updateCommitIndex] Updating commitIndex to " << index << std::endl;
            break;
        }
    }
    

    // 把这里注释了
    // 如果在这里直接调用 applyCommittedEntriesInternal()，可能会导致 Leader 在处理 AppendEntries 响应时长时间持锁，从而阻塞其他重要操作（如处理客户端请求、发送心跳等）。
    // 通过让上层应用（如 server）主动调用 applyCommittedEntries()，可以在适当的时机应用已提交的日志条目，同时避免在 Raft 内部持锁执行耗时操作。
    // 如果 commitIndex 有更新，立即应用
    // if (commitIndex_ > oldCommitIndex) {
        // std::cout << "[updateCommitIndex] commitIndex updated from " << oldCommitIndex 
        //           << " to " << commitIndex_ << ", applying entries on LEADER" << std::endl;
        // applyCommittedEntriesInternal();
        //
    // }
}

AppendEntriesArgs RaftNode::createAppendEntriesArgs(int nodeIndex) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 只有 Leader 可以创建 AppendEntries 参数
    if (state_ != NodeState::LEADER || nodeIndex < 0 || 
        nodeIndex >= static_cast<int>(peerIds_.size())) {
        return AppendEntriesArgs();
    }
    
    AppendEntriesArgs args;
    args.term = currentTerm_;
    args.leaderId = nodeId_;
    args.leaderCommit = commitIndex_;
    
    // 确定要发送的日志条目
    int nextIndex = nextIndex_[nodeIndex];
    args.prevLogIndex = nextIndex - 1;
    
    if (args.prevLogIndex > 0 && args.prevLogIndex < static_cast<int>(log_.size())) {
        args.prevLogTerm = log_[args.prevLogIndex].term;
    } else {
        args.prevLogTerm = 0;
    }
    
    // 添加从 nextIndex 开始的所有日志条目
    for (int i = nextIndex; i < static_cast<int>(log_.size()); ++i) {
        args.entries.push_back(log_[i]);
    }
    
    return args;
}

int RaftNode::getCommitIndex() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return commitIndex_;
}

int RaftNode::getLastApplied() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastApplied_;
}

void RaftNode::setApplyCallback(std::function<std::string(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    applyCallback_ = callback;
}

// ==================== 客户端请求接口 ====================

bool RaftNode::submitCommand(const std::string& command, std::string& result, int timeout_ms) {
    // 检查是否为 Leader
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != NodeState::LEADER) {
            std::cout << "[submitCommand] Not leader, rejecting command" << std::endl;
            return false;
        }
    }
    
    // 追加日志条目
    int logIndex = appendLogEntry(command);
    if (logIndex < 0) {
        std::cout << "[submitCommand] Failed to append log entry" << std::endl;
        return false;
    }
    
    std::cout << "[submitCommand] Command appended at index " << logIndex 
              << ", command: " << command << std::endl;
    
    // 检查是否是单节点集群
    bool isSingleNode = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        isSingleNode = (peerIds_.size() == 0);
        
        // 如果是单节点集群，立即更新 commitIndex
        if (isSingleNode) {
            commitIndex_ = logIndex;
            std::cout << "[submitCommand] Single node cluster, commitIndex updated to " << logIndex << std::endl;
        }
    }
    
    // 创建待处理命令
    auto pendingCmd = std::make_shared<PendingCommand>();
    pendingCmd->logIndex = logIndex;
    pendingCmd->command = command;
    pendingCmd->completed = false;
    
    {
        std::lock_guard<std::mutex> lock(pendingCommandsMutex_);
        pendingCommands_[logIndex] = pendingCmd;
        std::cout << "[submitCommand] PendingCommand created for index " << logIndex 
                  << ", total pending: " << pendingCommands_.size() << std::endl;
    }
    
    // 如果是单节点集群，立即应用命令
    if (isSingleNode) {
        std::cout << "[submitCommand] Applying committed entries for single node" << std::endl;
        applyCommittedEntries();
    }
    
    // 等待命令被提交和应用
    std::cout << "[submitCommand] Waiting for command at index " << logIndex 
              << " to be applied (timeout: " << timeout_ms << "ms)" << std::endl;
    
    std::unique_lock<std::mutex> lock(pendingCmd->mutex);
    bool success = pendingCmd->cv.wait_for(
        lock,
        std::chrono::milliseconds(timeout_ms),
        [&pendingCmd]() { return pendingCmd->completed; }
    );
    
    if (success) {
        result = pendingCmd->result;
        std::cout << "[submitCommand] Command at index " << logIndex 
                  << " completed successfully, result: " << result << std::endl;
    } else {
        std::cout << "[submitCommand] Command at index " << logIndex 
                  << " TIMED OUT after " << timeout_ms << "ms" << std::endl;
        
        // 打印当前状态用于调试
        std::lock_guard<std::mutex> stateLock(mutex_);
        std::cout << "[submitCommand] Current state - commitIndex: " << commitIndex_ 
                  << ", lastApplied: " << lastApplied_ 
                  << ", logSize: " << log_.size() << std::endl;
    }
    
    // 清理待处理命令
    {
        std::lock_guard<std::mutex> lock(pendingCommandsMutex_);
        pendingCommands_.erase(logIndex);
    }
    
    return success;
}

std::string RaftNode::getNodeAddress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodeAddress_;
}

void RaftNode::setNodeAddress(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    nodeAddress_ = address;
}

std::string RaftNode::getLeaderAddress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果自己是 Leader，返回自己的地址
    if (state_ == NodeState::LEADER) {
        return nodeAddress_;
    }
    
    // 如果知道 Leader ID，返回 Leader 地址
    if (leaderId_ >= 0) {
        auto it = peerAddresses_.find(leaderId_);
        if (it != peerAddresses_.end()) {
            return it->second;
        }
    }
    
    return "";  // 未知 Leader
}

void RaftNode::setPeerAddresses(const std::map<int, std::string>& peer_addresses) {
    std::lock_guard<std::mutex> lock(mutex_);
    peerAddresses_ = peer_addresses;
}

// ==================== 私有辅助方法 ====================

RaftNode::Duration RaftNode::generateElectionTimeout() {
    std::uniform_int_distribution<int> dist(ELECTION_TIMEOUT_MIN, ELECTION_TIMEOUT_MAX);
    return Duration(dist(rng_));
}

bool RaftNode::isLogUpToDate(int lastLogIndex, int lastLogTerm) const {
    // 注意：此方法假设调用者已持有锁
    
    int myLastLogIndex = getLastLogIndex();
    int myLastLogTerm = getLastLogTerm();
    
    // 比较规则：
    // 1. 如果任期不同，任期大的更新
    // 2. 如果任期相同，索引大的更新
    if (lastLogTerm != myLastLogTerm) {
        return lastLogTerm > myLastLogTerm;
    }
    return lastLogIndex >= myLastLogIndex;
}

// ==================== RPC 客户端管理 ====================

void* RaftNode::getRPCClient(int nodeId) {
    std::lock_guard<std::mutex> lock(rpcClientsMutex_);
    
    // 检查是否已存在客户端
    auto it = rpcClients_.find(nodeId);
    if (it != rpcClients_.end()) {
        return it->second.get();
    }
    
    // 获取节点地址
    auto addr_it = peerAddresses_.find(nodeId);
    if (addr_it == peerAddresses_.end()) {
        std::cerr << "Node " << nodeId_ << ": No address found for peer " << nodeId << std::endl;
        return nullptr;
    }
    
    // 创建新的 RPC 客户端
    try {
        auto client = network::createRPCClient(addr_it->second);
        if (!client || !client->connect(addr_it->second)) {
            std::cerr << "Node " << nodeId_ << ": Failed to connect to peer " << nodeId 
                     << " at " << addr_it->second << std::endl;
            return nullptr;
        }
        
        // 保存客户端（使用 shared_ptr<void> 存储）
        void* raw_ptr = client.release();
        rpcClients_[nodeId] = std::shared_ptr<void>(raw_ptr, [](void* p) {
            delete static_cast<network::RaftRPCClient*>(p);
        });
        
        return raw_ptr;
    } catch (const std::exception& e) {
        std::cerr << "Node " << nodeId_ << ": Exception creating RPC client for peer " 
                 << nodeId << ": " << e.what() << std::endl;
        return nullptr;
    }
}

void RaftNode::sendRequestVoteAsync(int nodeId) {
    // 获取 RPC 客户端
    void* client_ptr = getRPCClient(nodeId);
    if (!client_ptr) {
        return;
    }
    
    auto* client = static_cast<network::RaftRPCClient*>(client_ptr);
    
    // 准备请求参数（需要在锁外准备以避免死锁）
    RequestVoteArgs args;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        args.term = currentTerm_;
        args.candidateId = nodeId_;
        args.lastLogIndex = getLastLogIndex();
        args.lastLogTerm = getLastLogTerm();
    }
    
    // 发送 RPC
    RequestVoteReply reply;
    bool success = client->sendRequestVote(args, reply, 1000);
    
    if (success) {
        // 处理响应
        handleVoteResponse(nodeId, reply);
    } else {
        std::cerr << "Node " << nodeId_ << ": Failed to send RequestVote to peer " << nodeId << std::endl;
    }
}

void RaftNode::sendAppendEntriesAsync(int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(peerIds_.size())) {
        return;
    }
    
    int nodeId = peerIds_[nodeIndex];
    
    // 获取 RPC 客户端
    void* client_ptr = getRPCClient(nodeId);
    if (!client_ptr) {
        return;
    }
    
    auto* client = static_cast<network::RaftRPCClient*>(client_ptr);
    
    // 准备请求参数
    AppendEntriesArgs args;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        args = createAppendEntriesArgs(nodeIndex);
    }
    
    // 发送 RPC
    AppendEntriesReply reply;
    bool success = client->sendAppendEntries(args, reply, 1000);
    
    if (success) {
        // 处理响应
        bool should_update_commit = handleAppendEntriesResponse(nodeId, reply);
        
        if (should_update_commit) {
            std::lock_guard<std::mutex> lock(mutex_);
            updateCommitIndex();
            applyCommittedEntriesInternal();
        }
    } else {
        std::cerr << "Node " << nodeId_ << ": Failed to send AppendEntries to peer " << nodeId << std::endl;
    }
}

} // namespace raft
