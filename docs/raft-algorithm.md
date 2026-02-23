# Raft 算法详解

## 1. Raft 算法概述

Raft 是一种用于管理复制日志的共识算法。它被设计为易于理解的共识算法，与 Paxos 相比更容易实现和理解。

### 1.1 核心思想

- **Leader-based**: 系统中有一个明确的 Leader 负责管理日志复制
- **Strong Leader**: 日志条目只从 Leader 流向 Follower
- **Term-based**: 使用任期号来检测过时的信息

### 1.2 三个子问题

1. **Leader Election**: 如何选出一个 Leader
2. **Log Replication**: Leader 如何复制日志到其他节点
3. **Safety**: 如何保证已提交的日志不会丢失

## 2. 节点状态

### 2.1 三种状态

```
┌──────────┐
│ Follower │ ◄─────────────┐
└────┬─────┘                │
     │ timeout              │
     ▼                      │
┌───────────┐               │
│ Candidate │               │
└────┬──┬───┘               │
     │  │                   │
     │  └───────────────────┘
     │ receives votes       │
     │ from majority        │
     ▼                      │
┌─────────┐                 │
│  Leader │ ────────────────┘
└─────────┘  discovers higher term
```

**Follower (跟随者)**:
- 被动接收 RPC 请求
- 如果选举超时，转变为 Candidate

**Candidate (候选人)**:
- 发起选举，请求投票
- 如果获得多数票，成为 Leader
- 如果收到更高任期的消息，成为 Follower
- 如果超时，重新发起选举

**Leader (领导者)**:
- 处理所有客户端请求
- 复制日志到 Follower
- 定期发送心跳维持领导地位

### 2.2 任期 (Term)

```
Term 1    Term 2    Term 3    Term 4    Term 5
┌────┐    ┌────┐    ┌────┐    ┌────┐    ┌────┐
│ L1 │    │ L2 │    │ X  │    │ L3 │    │ L4 │
└────┘    └────┘    └────┘    └────┘    └────┘
         Election  Failed    Election
                   Election
```

- 任期是逻辑时钟，单调递增
- 每个任期最多有一个 Leader
- 某些任期可能没有 Leader（选举失败）
- 节点通过比较任期号来检测过时信息

## 3. 领导者选举

### 3.1 选举触发

Follower 在选举超时时间内未收到 Leader 的心跳，触发选举：

```
1. 增加 currentTerm
2. 转变为 Candidate
3. 投票给自己
4. 重置选举定时器
5. 向所有其他节点发送 RequestVote RPC
```

### 3.2 RequestVote RPC

**请求参数**:
```cpp
struct RequestVoteArgs {
    int term;           // 候选人的任期
    int candidateId;    // 候选人 ID
    int lastLogIndex;   // 候选人最后日志条目的索引
    int lastLogTerm;    // 候选人最后日志条目的任期
};
```

**响应参数**:
```cpp
struct RequestVoteReply {
    int term;           // 当前任期，用于候选人更新自己
    bool voteGranted;   // true 表示投票给候选人
};
```

**投票规则**:
```
投票给候选人，当且仅当：
1. 候选人的任期 >= 自己的任期
2. 自己在当前任期还没有投票，或已经投票给该候选人
3. 候选人的日志至少和自己一样新
```

### 3.3 日志新旧比较

日志 A 比日志 B 新，当且仅当：
```
(A.lastLogTerm > B.lastLogTerm) OR
(A.lastLogTerm == B.lastLogTerm AND A.lastLogIndex >= B.lastLogIndex)
```

### 3.4 选举结果

**成为 Leader**:
- 获得多数节点的投票
- 立即发送心跳给所有节点

**回到 Follower**:
- 收到更高任期的 AppendEntries 或 RequestVote

**选举超时**:
- 没有获得多数票
- 增加任期，重新发起选举

### 3.5 选举超时

- 使用随机超时时间（如 150-300ms）
- 避免多个节点同时发起选举（split vote）
- 通常选举超时 >> 心跳间隔

## 4. 日志复制

### 4.1 日志结构

```
Index:  1    2    3    4    5    6    7
Term:   1    1    1    2    3    3    3
Cmd:   x←1  y←2  x←3  y←4  x←5  y←6  x←7
```

每个日志条目包含：
- **Index**: 日志索引（从 1 开始）
- **Term**: 创建该条目时的任期
- **Command**: 状态机命令

### 4.2 AppendEntries RPC

**请求参数**:
```cpp
struct AppendEntriesArgs {
    int term;                      // Leader 的任期
    int leaderId;                  // Leader ID
    int prevLogIndex;              // 前一个日志条目的索引
    int prevLogTerm;               // 前一个日志条目的任期
    std::vector<LogEntry> entries; // 日志条目（心跳时为空）
    int leaderCommit;              // Leader 的 commitIndex
};
```

**响应参数**:
```cpp
struct AppendEntriesReply {
    int term;           // 当前任期
    bool success;       // 如果 Follower 包含匹配的日志，返回 true
    int conflictIndex;  // 冲突日志的索引（优化用）
    int conflictTerm;   // 冲突日志的任期（优化用）
};
```

### 4.3 日志复制流程

**Leader 端**:
```
1. 接收客户端请求
2. 追加日志到本地
3. 并行发送 AppendEntries 到所有 Follower
4. 等待多数节点响应
5. 提交日志（更新 commitIndex）
6. 应用到状态机
7. 返回结果给客户端
8. 下次心跳通知 Follower 提交
```

**Follower 端**:
```
1. 接收 AppendEntries RPC
2. 检查 prevLogIndex 和 prevLogTerm 是否匹配
3. 如果不匹配，返回 false
4. 如果匹配：
   a. 删除冲突的日志条目
   b. 追加新的日志条目
   c. 更新 commitIndex
   d. 返回 true
```

### 4.4 日志一致性检查

Leader 维护每个 Follower 的 `nextIndex`:
```
nextIndex[i]: 下一个要发送给 Follower i 的日志索引
```

**一致性保证**:
- 如果两个日志条目有相同的索引和任期，则它们存储相同的命令
- 如果两个日志条目有相同的索引和任期，则它们之前的所有日志都相同

### 4.5 处理日志冲突

当 Follower 的日志与 Leader 不一致时：

```
Leader:  1  1  1  4  4  5  5  6  6  6
Follower:1  1  1  4  4  5  5  5  5
                              ↑
                         冲突点
```

**回退策略**:
1. Follower 返回 success=false
2. Leader 递减 nextIndex[i]
3. 重新发送 AppendEntries
4. 重复直到找到匹配点

**优化**:
- Follower 返回冲突日志的任期和索引
- Leader 跳过整个冲突任期

### 4.6 日志提交

Leader 提交日志的条件：
```
1. 日志已复制到多数节点
2. 日志的任期等于当前任期
```

**为什么需要条件 2？**
- 防止提交旧任期的日志
- 保证 Leader Completeness Property

## 5. 安全性

### 5.1 选举限制 (Election Restriction)

**规则**: 只有包含所有已提交日志的节点才能被选为 Leader

**实现**: 通过日志新旧比较，拒绝投票给日志较旧的候选人

### 5.2 日志匹配特性 (Log Matching Property)

**规则**: 如果两个日志条目有相同的索引和任期，则：
1. 它们存储相同的命令
2. 它们之前的所有日志都相同

### 5.3 Leader 完整性 (Leader Completeness)

**规则**: 如果一个日志条目在某个任期被提交，那么这个条目将出现在所有更高任期的 Leader 的日志中

### 5.4 状态机安全性 (State Machine Safety)

**规则**: 如果一个节点已经应用了某个索引的日志条目到状态机，那么其他节点不会在该索引应用不同的日志条目

## 6. 持久化

### 6.1 需要持久化的状态

**所有节点**:
- `currentTerm`: 当前任期
- `votedFor`: 当前任期投票给了谁
- `log[]`: 日志条目数组

### 6.2 持久化时机

- 任期号变化时
- 投票时
- 追加日志时

### 6.3 崩溃恢复

节点重启后：
```
1. 从持久化存储恢复状态
2. 以 Follower 身份启动
3. 等待 Leader 的心跳或发起选举
4. 通过日志复制同步缺失的日志
```

## 7. 客户端交互

### 7.1 请求处理

```
1. 客户端发送请求到任意节点
2. 如果是 Follower，重定向到 Leader
3. Leader 将请求转换为日志条目
4. 执行日志复制流程
5. 日志提交后应用到状态机
6. 返回结果给客户端
```

### 7.2 线性一致性

**保证**: 客户端看到的操作顺序与实际执行顺序一致

**实现**:
- 只有 Leader 处理写请求
- 日志提交后才返回结果
- 使用客户端 ID 和序列号去重

### 7.3 只读优化

**问题**: 读请求也需要走日志复制流程吗？

**优化方案**:
1. **ReadIndex**: Leader 记录当前 commitIndex，等待应用后返回
2. **Lease Read**: Leader 在租约期内直接读取，无需等待

## 8. 常见问题

### 8.1 脑裂问题

**问题**: 网络分区导致多个 Leader？

**解决**: 多数派机制，只有获得多数票的节点才能成为 Leader

### 8.2 活锁问题

**问题**: 多个节点同时发起选举，导致无法选出 Leader？

**解决**: 随机选举超时时间

### 8.3 日志压缩

**问题**: 日志无限增长？

**解决**: Snapshot 机制，定期创建快照并清理旧日志

## 9. 性能优化

### 9.1 批量操作

- 批量追加日志
- 批量发送 RPC

### 9.2 Pipeline

- 不等待响应，连续发送多个 AppendEntries
- 减少 RTT 延迟

### 9.3 并行复制

- 并行发送 RPC 到所有 Follower
- 使用线程池或异步 I/O

## 10. 实现建议

### 10.1 调试技巧

- 详细的日志记录
- 状态可视化
- 单元测试覆盖核心逻辑

### 10.2 测试策略

- 正常场景测试
- 故障场景测试（节点崩溃、网络分区）
- 并发测试
- 性能测试

### 10.3 常见陷阱

- 忘记持久化状态
- 日志索引从 0 开始（应该从 1 开始）
- 提交旧任期的日志
- 死锁和竞态条件

## 参考资料

- [Raft 论文](https://raft.github.io/raft.pdf)
- [Raft 作者的博士论文](https://github.com/ongardie/dissertation)
- [Raft 可视化](https://raft.github.io/)
- [MIT 6.824 Raft Lab](https://pdos.csail.mit.edu/6.824/labs/lab-raft.html)
