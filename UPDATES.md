# 更新说明 - 完善网络 RPC 功能

## 问题描述

之前的 RaftNode 实现是为单元测试设计的，`startElection()` 和 `replicateLog()` 方法只处理状态转换，没有实际发送网络 RPC 请求。这导致系统无法真正作为分布式系统运行。

## 解决方案

### 1. 添加 RPC 客户端管理

在 `include/raft/raft_node.h` 中添加：

```cpp
// RPC 客户端管理
std::map<int, std::shared_ptr<void>> rpcClients_;  // 对等节点的 RPC 客户端
std::mutex rpcClientsMutex_;                       // 保护 rpcClients_
```

### 2. 添加辅助方法

```cpp
// 获取或创建到指定节点的 RPC 客户端
void* getRPCClient(int nodeId);

// 向指定节点发送 RequestVote RPC（异步）
void sendRequestVoteAsync(int nodeId);

// 向指定节点发送 AppendEntries RPC（异步）
void sendAppendEntriesAsync(int nodeIndex);
```

### 3. 完善 startElection() 方法

**之前**：
```cpp
void RaftNode::startElection() {
    // ... 状态转换逻辑 ...
    
    // 注意：实际的投票请求发送需要在外部实现（涉及网络通信）
    // 这里只处理状态转换逻辑
}
```

**现在**：
```cpp
void RaftNode::startElection() {
    // ... 状态转换逻辑 ...
    
    // 向所有对等节点发送 RequestVote RPC
    for (size_t i = 0; i < peerIds_.size(); ++i) {
        std::thread([this, i]() {
            sendRequestVoteAsync(peerIds_[i]);
        }).detach();
    }
}
```

### 4. 完善 replicateLog() 方法

**之前**：
```cpp
void RaftNode::replicateLog() {
    // ... 检查状态 ...
    
    // 注意：实际的 RPC 发送需要在外部实现（涉及网络通信）
    // 这里只准备 AppendEntries 参数
}
```

**现在**：
```cpp
void RaftNode::replicateLog() {
    // ... 检查状态 ...
    
    // 向所有对等节点发送 AppendEntries RPC
    for (size_t i = 0; i < peerIds_.size(); ++i) {
        std::thread([this, i]() {
            sendAppendEntriesAsync(i);
        }).detach();
    }
}
```

### 5. 实现 RPC 客户端管理方法

在 `src/raft/raft_node.cpp` 中实现：

- `getRPCClient(int nodeId)` - 获取或创建 RPC 客户端
- `sendRequestVoteAsync(int nodeId)` - 异步发送投票请求
- `sendAppendEntriesAsync(int nodeIndex)` - 异步发送日志复制请求

## 关键改进

### 1. 自动 RPC 客户端管理

- 延迟创建：只在需要时创建 RPC 客户端
- 连接复用：同一节点的客户端被缓存和复用
- 自动清理：使用 shared_ptr 自动管理生命周期

### 2. 异步 RPC 调用

- 使用独立线程发送 RPC，避免阻塞主循环
- 自动处理响应（调用 handleVoteResponse 或 handleAppendEntriesResponse）
- 错误处理和日志记录

### 3. 线程安全

- 使用互斥锁保护 RPC 客户端映射
- 在锁外准备 RPC 参数，避免死锁
- 正确处理并发访问

## 使用示例

### 启动集群

```bash
# 1. 编译项目
rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc)
cd ..

# 2. 启动集群
chmod +x scripts/*.sh
./scripts/start_cluster.sh

# 3. 查看日志（应该能看到选举和心跳）
tail -f logs/node*.log
```

### 预期行为

启动后，你应该看到：

1. **选举过程**：
   ```
   Node 1: Starting election for term 1
   Node 1: Sending RequestVote to peer 2
   Node 1: Sending RequestVote to peer 3
   Node 1: Received vote from peer 2
   Node 1: Became LEADER for term 1
   ```

2. **心跳发送**：
   ```
   Node 1: Sending heartbeat to all followers
   Node 1: AppendEntries success from peer 2
   Node 1: AppendEntries success from peer 3
   ```

3. **日志复制**：
   ```
   Node 1: Appending log entry: PUT:key1:value1
   Node 1: Replicating log to followers
   Node 1: Log replicated to majority, committing index 1
   ```

## 测试验证

### 1. 测试选举

```bash
# 启动 3 节点集群
./scripts/start_cluster.sh

# 等待选举完成
sleep 3

# 检查状态（应该有一个 Leader）
./scripts/check_cluster.sh
grep "LEADER" logs/node*.log
```

### 2. 测试日志复制

```bash
# 使用客户端写入数据
./build/bin/kv_client_cli
raft-kv> put test1 value1
OK
raft-kv> quit

# 检查所有节点的日志
grep "Appending log entry" logs/node*.log
grep "Commit Index" logs/node*.log
```

### 3. 测试故障恢复

```bash
# 停止 Leader
kill $(cat logs/node1.pid)

# 等待新选举
sleep 1

# 应该选出新 Leader
grep "Became LEADER" logs/node*.log
```

## 注意事项

### 1. 线程管理

- RPC 调用使用 detached 线程，确保不会阻塞主循环
- 每个 RPC 调用都是独立的，失败不会影响其他调用

### 2. 错误处理

- RPC 失败会记录错误日志但不会崩溃
- 连接失败会自动重试（在下次 RPC 调用时）

### 3. 性能考虑

- RPC 客户端被缓存，避免重复创建连接
- 使用异步调用，提高并发性能
- 超时设置为 1000ms，可根据网络情况调整

## 已知限制

1. **无连接池**：每个节点只维护一个到对等节点的连接
2. **无重试机制**：RPC 失败不会自动重试（依赖下次心跳）
3. **无流量控制**：没有限制并发 RPC 数量
4. **无监控指标**：缺少 RPC 成功率、延迟等指标

## 下一步

这些限制可以在后续的优化阶段（任务 10）中改进：

- 实现连接池和重试机制
- 添加流量控制和背压
- 集成监控和指标收集
- 优化 RPC 性能

## 编译和运行

请参考 [COMPILE_INSTRUCTIONS.md](COMPILE_INSTRUCTIONS.md) 获取详细的编译步骤。

## 总结

通过这次更新，RaftNode 现在可以：

✅ 自动发送 RequestVote RPC 进行选举
✅ 自动发送 AppendEntries RPC 复制日志和心跳
✅ 管理到对等节点的 RPC 客户端连接
✅ 异步处理 RPC 调用，不阻塞主循环
✅ 正确处理 RPC 响应和错误

系统现在是一个真正的分布式 Raft KV 存储系统！
