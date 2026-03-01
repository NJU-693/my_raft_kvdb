# Task 9.2.4 - 故障恢复测试结果

## 测试概述

测试持久化功能和节点崩溃后的恢复能力。

## 测试环境

- 集群规模：3 节点
- 测试脚本：`scripts/test_crash_recovery.sh`
- 测试时间：2026-03-01

## 测试场景

### 场景 1：节点崩溃与恢复

**测试步骤：**

1. 启动 3 节点集群
2. 写入 5 个键值对（key1-key3, user1-user2）
3. 验证数据写入成功
4. 检查持久化文件是否存在
5. 模拟 Node 2 崩溃（kill -9）
6. 在 Node 2 离线期间写入更多数据（key4-key5）
7. 重启 Node 2
8. 验证 Node 2 恢复状态并追赶日志
9. 验证所有数据一致性

**测试结果：**

```
========================================
  Test Results
========================================
Total operations: 7
Successful: 7
Failed: 0
Success rate: 100.0%

✓ All tests passed!

Crash recovery test completed successfully:
  1. Node 2 crashed and was killed
  2. Node 2 restarted and recovered state from disk
  3. Node 2 caught up with missed log entries
  4. All data is consistent across the cluster
```

**持久化文件验证：**

```
Step 7: Checking persistent storage files...
  Node 1: raft_state.dat exists (size: 339 bytes)
  Node 2: raft_state.dat exists (size: 339 bytes)
  Node 3: raft_state.dat exists (size: 340 bytes)
```

所有节点都成功创建了持久化文件。

**数据一致性验证：**

| Key | Expected Value | Actual Value | Status |
|-----|---------------|--------------|--------|
| key1 | value1 | value1 | ✓ |
| key2 | value2 | value2 | ✓ |
| key3 | value3 | value3 | ✓ |
| key4 | value4 | value4 | ✓ |
| key5 | value5 | value5 | ✓ |
| user1 | Alice | Alice | ✓ |
| user2 | Bob | Bob | ✓ |

所有数据在节点恢复后保持一致。

## 持久化实现细节

### 持久化内容

RaftNode 持久化以下状态（符合 Raft 论文要求）：

1. `currentTerm`：当前任期号
2. `votedFor`：当前任期投票给的节点 ID（-1 表示未投票）
3. `log[]`：日志条目数组

### 持久化时机

系统在以下关键时刻调用 `persistState()`：

1. **handleRequestVote()**：当任期变化或投票被授予时
2. **handleAppendEntries()**：当任期变化或日志被修改时
3. **startElection()**：当任期递增并投票给自己时
4. **handleVoteResponse()**：当发现更高任期时
5. **appendLogEntry()**：当新日志条目被添加时
6. **becomeFollower()**：当任期变化时
7. **handleAppendEntriesResponse()**：当发现更高任期时

### 恢复机制

节点启动时：

1. 创建 `Persister` 实例
2. 调用 `setPersister()` 设置持久化器
3. 调用 `restoreState()` 从磁盘恢复状态
4. 如果恢复成功，使用持久化的 term、votedFor 和 log
5. 如果恢复失败（首次启动），使用默认初始状态

### 原子性保证

`Persister` 使用原子性写入策略：

1. 先写入临时文件（`raft_state.dat.tmp`）
2. 写入成功后重命名为正式文件（`raft_state.dat`）
3. 避免写入过程中崩溃导致的数据损坏

## 测试结论

✅ **持久化功能正常工作**

- 节点崩溃后能够从磁盘恢复状态
- 恢复的状态包括 term、votedFor 和完整的日志
- 节点重启后能够正确追赶遗漏的日志条目
- 所有数据在恢复后保持一致

✅ **符合 Raft 论文要求**

- 持久化了所有必需的状态（currentTerm, votedFor, log[]）
- 在所有关键时刻进行持久化
- 使用原子性写入保证数据完整性

✅ **系统可靠性验证**

- 节点崩溃不会导致数据丢失
- 集群在节点故障期间继续提供服务
- 故障节点恢复后能够重新加入集群

## 性能观察

- 持久化文件大小：约 340 字节（5 个日志条目）
- 节点恢复时间：< 3 秒
- 日志追赶时间：< 5 秒（2 个遗漏的条目）

## 后续改进建议

1. **日志压缩（Snapshot）**：当日志过大时，实现快照机制减少恢复时间
2. **批量持久化**：合并多个持久化操作，减少磁盘 I/O
3. **异步持久化**：使用后台线程进行持久化，避免阻塞主线程
4. **校验和**：在持久化数据中添加校验和，检测数据损坏

## 相关文件

- 测试脚本：`scripts/test_crash_recovery.sh`
- 持久化实现：`include/storage/persister.h`, `src/storage/persister.cpp`
- RaftNode 集成：`include/raft/raft_node.h`, `src/raft/raft_node.cpp`
- 服务器集成：`src/raft_node_server.cpp`
