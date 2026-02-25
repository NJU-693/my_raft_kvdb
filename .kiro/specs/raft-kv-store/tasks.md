# Raft KV 分布式存储系统 - 实现任务

## 阶段一：基础框架搭建

### 1. 项目初始化
- [x] 1.1 创建项目目录结构
- [x] 1.2 配置 CMake 构建系统
- [x] 1.3 添加基本的 README 和文档框架
- [x] 1.4 配置代码格式化工具（clang-format）

### 2. 跳表数据结构实现
- [x] 2.1 实现跳表节点结构
  - 定义模板化的 SkipListNode
  - 实现多层前向指针
- [x] 2.2 实现跳表类基本框架
  - 构造函数和析构函数
  - 随机层数生成函数
- [x] 2.3 实现跳表查找操作
  - 从最高层开始查找
  - 返回查找结果
- [x] 2.4 实现跳表插入操作
  - 查找插入位置
  - 更新各层指针
  - 处理重复 key
- [x] 2.5 实现跳表删除操作
  - 查找目标节点
  - 更新各层指针
  - 释放内存
- [x] 2.6 实现跳表辅助功能
  - 获取大小
  - 显示结构（调试用）
- [x] 2.7 编写跳表单元测试
  - 测试插入、查找、删除
  - 测试边界情况
  - 性能测试

### 3. KV 存储引擎实现
- [x] 3.1 实现命令结构
  - 定义 CommandType 枚举（PUT, GET, DELETE）
  - 实现 Command 类及序列化/反序列化方法
  - _需求: 2.2.2_

- [x] 3.2 实现 KVStore 类基础框架
  - 创建 include/storage/kv_store.h 和 src/storage/kv_store.cpp
  - 基于 SkipList<string, string> 实现存储
  - 添加 mutex 保护并发访问
  - 实现构造函数和析构函数
  - _需求: 2.2.1, 2.2.2_

- [x] 3.3 实现 KV 基本操作方法
  - 实现 put(key, value) 方法
  - 实现 get(key, value&) 方法
  - 实现 remove(key) 方法
  - 确保线程安全（使用 lock_guard）
  - _需求: 2.2.2_

- [x] 3.4 实现命令应用接口
  - 实现 applyCommand(const string& command) 方法
  - 实现 applyCommandWithResult(const string& command) 方法
  - 解析命令字符串格式（PUT:key:value, GET:key, DELETE:key）
  - 调用相应的 KV 操作方法
  - _需求: 2.2.2_

- [x] 3.5 编写 KV 存储单元测试
  - 创建 test/kv_store_test.cpp
  - 测试 Put/Get/Delete 基本操作
  - 测试并发访问场景
  - 测试命令应用接口
  - _需求: 2.2.2_

### 4. 基础工具模块（可选，按需实现）
- [ ] 4.1 实现日志系统
  - 创建 include/util/logger.h 和 src/util/logger.cpp
  - 支持不同日志级别（DEBUG, INFO, WARN, ERROR）
  - 支持输出到文件和控制台
  - _需求: 3.4_

- [ ] 4.2 实现配置管理
  - 创建 include/util/config.h 和 src/util/config.cpp
  - 读取配置文件并解析集群配置
  - _需求: 2.4.1_

## 阶段二：Raft 核心实现（后续阶段）

### 5. Raft 基础组件
- [x] 5.1 实现日志条目和 RPC 消息结构
  - 创建 include/raft/log_entry.h（LogEntry 类）
  - 创建 include/raft/rpc_messages.h（RequestVote 和 AppendEntries 消息）
  - 实现序列化和反序列化方法
  - _需求: 2.1.2, 2.3.1_

- [x] 5.2 实现 RaftNode 基本框架
  - 创建 include/raft/raft_node.h 和 src/raft/raft_node.cpp
  - 定义节点状态（FOLLOWER, CANDIDATE, LEADER）
  - 定义持久化状态（currentTerm, votedFor, log）
  - 定义易失状态（commitIndex, lastApplied, nextIndex, matchIndex）
  - 实现定时器机制（选举超时、心跳）
  - _需求: 2.1.1, 2.1.2_

### 6. 领导者选举和日志复制
- [x] 6.1 实现领导者选举逻辑
  - 实现 Follower/Candidate/Leader 状态转换
  - 实现 RequestVote RPC 处理
  - 实现选举超时和投票逻辑
  - _需求: 2.1.1_

- [x] 6.2 编写选举逻辑测试
  - 创建 test/raft_node_test.cpp 和 test/leader_election_integration_test.cpp
  - 测试正常选举流程和分裂投票场景
  - _需求: 2.1.1_

- [x] 6.3 实现日志复制逻辑
  - 实现日志追加和 AppendEntries RPC 处理
  - 实现日志一致性检查和冲突处理
  - 实现日志提交和应用逻辑
  - _需求: 2.1.2_

- [x] 6.4 编写日志复制测试
  - 创建 test/log_replication_test.cpp 和 test/log_replication_integration_test.cpp
  - 测试日志复制和冲突处理
  - _需求: 2.1.2_

### 7. 持久化实现
- [x] 7.1 实现 Persister 类
  - 创建 include/storage/persister.h 和 src/storage/persister.cpp
  - 实现 Raft 状态的保存和加载
  - 实现原子性写入和数据完整性保证
  - _需求: 2.1.3_

- [x] 7.2 编写持久化测试
  - 创建 test/persister_test.cpp
  - 测试崩溃恢复场景
  - _需求: 2.1.3_

## 阶段三：网络通信实现（后续阶段）

### 8. RPC 框架和客户端接口
- [x] 8.1 实现 RPC 框架
  - 实现 gRPC 方案（grpc_rpc_server.cpp, grpc_rpc_client.cpp）
  - 实现简单 RPC 方案（simple_rpc_server.cpp, simple_rpc_client.cpp）
  - 实现 RPC 工厂模式（rpc_factory.cpp）
  - 实现消息序列化和超时重试机制
  - _需求: 2.3.1_

- [x] 8.2 编写网络通信测试
  - 创建 test/rpc_test.cpp
  - 测试 RPC 调用和错误处理
  - _需求: 2.3.1_

- [x] 8.3 实现客户端接口
  - 创建 include/client/kv_client.h 和 src/client/kv_client.cpp
  - 实现客户端请求处理和重定向
  - 创建客户端库（KVClient）
  - _需求: 2.3.2_

- [x] 8.4 编写客户端测试
  - 创建 test/kv_client_test.cpp
  - 测试 KV 操作和重定向逻辑
  - _需求: 2.3.2_

## 阶段四：系统集成

### 9. 集成和测试
- [x] 9.1 系统集成
  - 集成 Raft 和 KV 存储
  - 实现节点启动流程和优雅关闭（raft_node_server.cpp）
  - 实现配置文件解析（util/config.h, util/config.cpp）
  - 编写配置文件（config/cluster.conf）
  - 创建集群管理脚本（scripts/start_cluster.sh, stop_cluster.sh, restart_cluster.sh, check_cluster.sh）
  - 创建客户端命令行工具（kv_client_cli.cpp）
  - 编写部署指南（docs/deployment-guide.md）
  - 完善 RaftNode 的网络 RPC 调用（添加 sendRequestVoteAsync, sendAppendEntriesAsync）
  - _需求: 2.1.2, 2.2.2, 2.4.1, 3.2_

- [ ] 9.2 集成测试和问题修复
  - [x] 9.2.1 修复 GET 请求超时问题
    - 调试 submitCommand() 中的条件变量等待逻辑（src/raft/raft_node.cpp:630-695）
    - 检查 applyCommittedEntriesInternal() 中的通知时序（src/raft/raft_node.cpp:501-540）
    - 验证 PendingCommand 的通知机制是否正确
    - 测试修复后的 GET 操作性能和稳定性
    - _需求: 2.3.2, 3.2_
  
  - [ ] 9.2.2 多节点选举测试
    - 测试 3 节点集群的领导者选举
    - 测试 5 节点集群的领导者选举
    - 测试领导者故障后的重新选举
    - _需求: 2.1.1_
  
  - [ ] 9.2.3 多节点日志复制测试
    - 测试正常情况下的日志复制
    - 测试 Follower 落后时的日志追赶
    - 测试日志冲突的解决
    - _需求: 2.1.2_
  
  - [ ] 9.2.4 故障恢复测试
    - 测试节点崩溃后的恢复（持久化验证）
    - 测试网络分区场景
    - 测试脑裂预防机制
    - _需求: 2.1.3, 3.1_
  
  - [ ] 9.2.5 并发客户端测试
    - 测试多客户端并发读写
    - 测试高负载下的系统稳定性
    - 测试客户端重定向逻辑
    - _需求: 2.3.2, 3.2_

- [ ] 9.3 文档完善
  - 更新架构文档（docs/architecture.md）
  - 更新算法说明文档（docs/raft-algorithm.md 等）
  - 编写快速开始指南（examples/QUICKSTART.md）
  - 编写开发文档和 API 文档
  - _需求: 3.3_

## 阶段五：优化与扩展（可选）

### 10. 性能优化和功能扩展
- [ ] 10.1 性能优化
  - 实现批量操作支持
  - 实现 Pipeline 优化
  - 实现并行日志复制
  - 进行性能测试和调优
  - _需求: 3.1_

- [ ] 10.2 功能扩展
  - 实现日志压缩（Snapshot）
  - 实现只读优化（ReadIndex/Lease Read）
  - 实现集群成员变更（动态添加/删除节点）
  - 实现监控和指标收集功能
  - _需求: 3.4, 2.4.2_

- [ ] 10.3 代码质量提升
  - 代码审查和重构
  - 增加测试覆盖率（目标 >80%）
  - 完善错误处理和日志记录
  - 性能分析和内存泄漏检查
  - 添加压力测试和混沌测试
  - _需求: 3.1, 3.2, 3.3_

## 当前进度

已完成：
- ✅ 阶段一：基础框架搭建（任务 1-3）
  - 项目初始化、跳表数据结构、KV 存储引擎
- ✅ 阶段二：Raft 核心实现（任务 5-7）
  - Raft 基础组件、领导者选举、日志复制、持久化
- ✅ 阶段三：网络通信实现（任务 8）
  - RPC 框架（gRPC + Simple RPC）、客户端接口
- ✅ 阶段四：系统集成 - 任务 9.1（已完成）
  - Raft 与 KV 存储集成完成
  - 节点服务器程序（raft_node_server.cpp）
  - 客户端命令行工具（kv_client_cli.cpp）
  - 集群管理脚本（start/stop/restart/check）
  - 网络 RPC 功能完善（sendRequestVoteAsync, sendAppendEntriesAsync）
  - 命令解析修复（支持包含冒号的 key，如 user:1）
  - 编译问题修复（库依赖和构建顺序）
  - 完整文档（15+ markdown 文件）
  - 系统验证：3 节点集群启动、领导者选举、日志复制、PUT/DELETE 操作正常

已知问题：
- ⚠️ GET 请求超时问题（"Max retries exceeded"）
  - 现象：PUT/DELETE 正常工作，但 GET 操作频繁超时
  - 根本原因：submitCommand() 中条件变量通知时序问题
  - 位置：src/raft/raft_node.cpp（submitCommand 和 applyCommittedEntriesInternal）
  - 影响：不影响系统正确性（集群正常运行、日志正常复制），但影响 GET 性能
  - 优先级：高（任务 9.2.1 首要解决）

当前重点：
- 🎯 任务 9.2.1: 修复 GET 请求超时问题（最高优先级）
- 🎯 任务 9.2.2-9.2.5: 完成集成测试（选举、复制、故障恢复、并发）
- 🎯 任务 9.3: 文档完善（更新架构文档、编写使用文档）

下一步：
- 阶段五：性能优化与功能扩展（可选）
  - 日志压缩（Snapshot）、只读优化、集群成员变更、监控功能
