# Raft KV 分布式存储系统 - 设计文档

## 1. 系统架构

### 1.1 整体架构
```
┌─────────────────────────────────────────────────────────┐
│                      客户端层                            │
│                   (Client Library)                      │
└────────────────────┬────────────────────────────────────┘
                     │
                     │ RPC 请求
                     ▼
┌─────────────────────────────────────────────────────────┐
│                    Raft 集群                             │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│  │  Node 1  │    │  Node 2  │    │  Node 3  │          │
│  │ (Leader) │◄──►│(Follower)│◄──►│(Follower)│          │
│  └──────────┘    └──────────┘    └──────────┘          │
└─────────────────────────────────────────────────────────┘

每个节点内部结构：
┌─────────────────────────────────────┐
│           Raft Node                 │
│  ┌─────────────────────────────┐   │
│  │    RPC 服务层                │   │
│  │  - RequestVote Handler      │   │
│  │  - AppendEntries Handler    │   │
│  │  - Client Request Handler   │   │
│  └─────────────────────────────┘   │
│  ┌─────────────────────────────┐   │
│  │    Raft 核心模块             │   │
│  │  - 状态机                    │   │
│  │  - 选举逻辑                  │   │
│  │  - 日志复制                  │   │
│  │  - 定时器                    │   │
│  └─────────────────────────────┘   │
│  ┌─────────────────────────────┐   │
│  │    存储层                    │   │
│  │  - 日志存储                  │   │
│  │  - 状态持久化                │   │
│  │  - KV 引擎 (跳表)            │   │
│  └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

### 1.2 模块划分

#### 1.2.1 核心模块
1. **Raft 模块** (`raft/`)
   - 状态管理
   - 选举逻辑
   - 日志复制
   - 持久化

2. **存储模块** (`storage/`)
   - 跳表实现
   - KV 引擎
   - 日志存储

3. **网络模块** (`network/`)
   - RPC 服务
   - 消息序列化
   - 网络通信

4. **工具模块** (`util/`)
   - 日志系统
   - 配置管理
   - 线程池

## 2. 详细设计

### 2.1 跳表实现

#### 2.1.1 数据结构设计
```cpp
// 跳表节点
template<typename K, typename V>
struct SkipListNode {
    K key;
    V value;
    std::vector<SkipListNode*> forward;  // 各层的前向指针
    
    SkipListNode(K k, V v, int level) 
        : key(k), value(v), forward(level + 1, nullptr) {}
};

// 跳表类
template<typename K, typename V>
class SkipList {
private:
    static constexpr int MAX_LEVEL = 16;      // 最大层数
    static constexpr float PROBABILITY = 0.5;  // 晋升概率
    
    SkipListNode<K, V>* header;  // 头节点
    int currentLevel;             // 当前最高层数
    int size;                     // 元素数量
    std::mt19937 rng;            // 随机数生成器
    
    int randomLevel();            // 生成随机层数
    
public:
    SkipList();
    ~SkipList();
    
    bool insert(const K& key, const V& value);
    bool search(const K& key, V& value);
    bool remove(const K& key);
    int getSize() const;
    void display() const;  // 调试用
};
```

#### 2.1.2 关键算法
- **查找**: 从最高层开始，向右查找直到下一个节点的 key 大于目标 key，然后下降一层
- **插入**: 先查找插入位置，记录每层的前驱节点，然后随机生成层数并插入
- **删除**: 查找目标节点，更新各层的指针，删除节点

### 2.2 Raft 状态机

#### 2.2.1 节点状态
```cpp
enum class NodeState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

class RaftNode {
private:
    // 持久化状态（所有服务器）
    int currentTerm;              // 当前任期
    int votedFor;                 // 当前任期投票给了谁
    std::vector<LogEntry> log;    // 日志条目
    
    // 易失状态（所有服务器）
    int commitIndex;              // 已提交的最高日志索引
    int lastApplied;              // 已应用到状态机的最高日志索引
    
    // 易失状态（Leader）
    std::vector<int> nextIndex;   // 每个服务器的下一个日志索引
    std::vector<int> matchIndex;  // 每个服务器已复制的最高日志索引
    
    // 节点状态
    NodeState state;
    int leaderId;
    
    // 定时器
    std::chrono::milliseconds electionTimeout;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartbeat;
    
public:
    void startElection();         // 发起选举
    void becomeLeader();          // 成为 Leader
    void becomeFollower(int term); // 成为 Follower
    void handleRequestVote(const RequestVoteArgs& args, RequestVoteReply& reply);
    void handleAppendEntries(const AppendEntriesArgs& args, AppendEntriesReply& reply);
    void replicateLog();          // 复制日志
    void applyCommittedEntries(); // 应用已提交的日志
};
```

#### 2.2.2 日志条目结构
```cpp
struct LogEntry {
    int term;                     // 日志条目的任期
    std::string command;          // 状态机命令（序列化的 KV 操作）
    int index;                    // 日志索引
};
```

#### 2.2.3 RPC 消息结构
```cpp
// RequestVote RPC
struct RequestVoteArgs {
    int term;                     // 候选人的任期
    int candidateId;              // 候选人 ID
    int lastLogIndex;             // 候选人最后日志条目的索引
    int lastLogTerm;              // 候选人最后日志条目的任期
};

struct RequestVoteReply {
    int term;                     // 当前任期
    bool voteGranted;             // 是否投票
};

// AppendEntries RPC
struct AppendEntriesArgs {
    int term;                     // Leader 的任期
    int leaderId;                 // Leader ID
    int prevLogIndex;             // 前一个日志条目的索引
    int prevLogTerm;              // 前一个日志条目的任期
    std::vector<LogEntry> entries; // 日志条目（心跳时为空）
    int leaderCommit;             // Leader 的 commitIndex
};

struct AppendEntriesReply {
    int term;                     // 当前任期
    bool success;                 // 是否成功
    int conflictIndex;            // 冲突日志的索引（优化用）
    int conflictTerm;             // 冲突日志的任期（优化用）
};
```

### 2.3 KV 存储引擎

#### 2.3.1 KV 引擎接口
```cpp
class KVStore {
private:
    SkipList<std::string, std::string> store;
    std::mutex mtx;  // 保护并发访问
    
public:
    bool put(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);
    bool remove(const std::string& key);
    
    // 应用日志条目
    void applyCommand(const std::string& command);
};
```

#### 2.3.2 命令格式
```cpp
// 命令序列化格式（简单的文本格式）
// PUT:key:value
// GET:key
// DELETE:key

enum class CommandType {
    PUT,
    GET,
    DELETE
};

struct Command {
    CommandType type;
    std::string key;
    std::string value;  // 仅 PUT 使用
    
    std::string serialize() const;
    static Command deserialize(const std::string& str);
};
```

### 2.4 网络通信

#### 2.4.1 RPC 框架选择
建议使用以下方案之一：
1. **gRPC + Protocol Buffers**（推荐）
   - 优点：成熟稳定，自动生成代码，支持多语言
   - 缺点：依赖较重
   
2. **原生 Socket + JSON**
   - 优点：轻量级，易于理解
   - 缺点：需要手动处理序列化和网络细节

#### 2.4.2 RPC 服务接口
```cpp
class RaftRPCService {
public:
    virtual RequestVoteReply requestVote(const RequestVoteArgs& args) = 0;
    virtual AppendEntriesReply appendEntries(const AppendEntriesArgs& args) = 0;
};

class RaftRPCClient {
public:
    virtual RequestVoteReply sendRequestVote(int nodeId, const RequestVoteArgs& args) = 0;
    virtual AppendEntriesReply sendAppendEntries(int nodeId, const AppendEntriesArgs& args) = 0;
};
```

### 2.5 持久化存储

#### 2.5.1 持久化内容
需要持久化的状态：
1. `currentTerm`：当前任期
2. `votedFor`：投票记录
3. `log[]`：日志条目

#### 2.5.2 持久化方案
```cpp
class Persister {
private:
    std::string dataDir;  // 数据目录
    
public:
    void saveRaftState(int currentTerm, int votedFor, const std::vector<LogEntry>& log);
    bool loadRaftState(int& currentTerm, int& votedFor, std::vector<LogEntry>& log);
    
    // 简单实现：使用文件存储
    // 优化实现：使用 LevelDB 或 RocksDB
};
```

## 3. 关键流程

### 3.1 领导者选举流程
```
1. Follower 启动选举定时器
2. 超时后转变为 Candidate
3. 增加 currentTerm
4. 投票给自己
5. 并行发送 RequestVote RPC 给所有节点
6. 等待结果：
   a. 获得多数票 → 成为 Leader
   b. 收到更高任期的 AppendEntries → 成为 Follower
   c. 超时 → 重新发起选举
```

### 3.2 日志复制流程
```
1. Leader 接收客户端请求
2. 追加日志到本地 log
3. 并行发送 AppendEntries RPC 给所有 Follower
4. Follower 检查日志一致性
5. Follower 追加日志并返回成功
6. Leader 收到多数成功响应
7. Leader 提交日志（更新 commitIndex）
8. Leader 应用日志到状态机
9. Leader 返回结果给客户端
10. 下次心跳通知 Follower 提交日志
```

### 3.3 客户端请求流程
```
1. 客户端发送请求到任意节点
2. 如果是 Follower，重定向到 Leader
3. Leader 将请求转换为日志条目
4. 执行日志复制流程
5. 日志提交后应用到 KV 存储
6. 返回结果给客户端
```

## 4. 项目结构

```
raft-kv-store/
├── CMakeLists.txt
├── README.md
├── docs/                    # 文档
│   ├── architecture.md      # 架构说明
│   ├── raft-algorithm.md    # Raft 算法详解
│   └── skiplist.md          # 跳表实现说明
├── include/                 # 头文件
│   ├── raft/
│   │   ├── raft_node.h
│   │   ├── log_entry.h
│   │   └── rpc_messages.h
│   ├── storage/
│   │   ├── skiplist.h
│   │   ├── kv_store.h
│   │   └── persister.h
│   ├── network/
│   │   ├── rpc_service.h
│   │   └── rpc_client.h
│   └── util/
│       ├── logger.h
│       ├── config.h
│       └── thread_pool.h
├── src/                     # 源文件
│   ├── raft/
│   ├── storage/
│   ├── network/
│   └── util/
├── test/                    # 测试
│   ├── skiplist_test.cpp
│   ├── raft_test.cpp
│   └── integration_test.cpp
├── examples/                # 示例
│   ├── single_node.cpp
│   └── cluster.cpp
└── config/                  # 配置文件
    └── cluster.conf
```

## 5. 配置文件设计

```ini
# cluster.conf
[cluster]
node_count = 3

[node_1]
id = 1
host = 127.0.0.1
port = 8001
data_dir = ./data/node1

[node_2]
id = 2
host = 127.0.0.1
port = 8002
data_dir = ./data/node2

[node_3]
id = 3
host = 127.0.0.1
port = 8003
data_dir = ./data/node3

[raft]
election_timeout_min = 150    # 毫秒
election_timeout_max = 300    # 毫秒
heartbeat_interval = 50       # 毫秒
```

## 6. 测试策略

### 6.1 单元测试
- 跳表基本操作测试
- Raft 状态转换测试
- 日志复制逻辑测试
- RPC 消息序列化测试

### 6.2 集成测试
- 单节点 KV 操作测试
- 多节点选举测试
- 日志复制一致性测试
- 故障恢复测试

### 6.3 性能测试
- 吞吐量测试
- 延迟测试
- 并发测试

## 7. 开发建议

### 7.1 开发顺序
1. 实现跳表数据结构（独立模块，易于测试）
2. 实现单节点 KV 存储引擎
3. 实现 Raft 状态机（不含网络）
4. 实现网络通信模块
5. 集成 Raft 和 KV 存储
6. 多节点测试和调试

### 7.2 调试技巧
- 使用详细的日志记录每个状态转换
- 实现可视化工具显示集群状态
- 使用单元测试验证核心逻辑
- 从简单场景开始（3 节点，无故障）

### 7.3 学习资源
- 阅读 Raft 论文并理解每个细节
- 参考 MIT 6.824 课程的 Lab 实现
- 研究开源实现（如 etcd 的 Raft 库）
- 使用 Raft 可视化工具理解算法

## 8. 未来扩展

### 8.1 功能扩展
- 日志压缩（Snapshot）
- 集群成员变更
- 只读优化
- 批量操作
- 事务支持

### 8.2 性能优化
- 并行日志复制
- Pipeline 优化
- 批量提交
- 零拷贝优化

### 8.3 运维功能
- 监控指标
- 管理接口
- 配置热更新
- 自动化测试框架
