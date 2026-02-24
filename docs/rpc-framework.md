# RPC 框架使用指南

## 概述

本项目实现了一个轻量级的 RPC（远程过程调用）框架，用于 Raft 节点之间的通信。当前实现基于原生 TCP Socket，提供了简单、高效的消息传递机制。

## 架构设计

### 核心组件

1. **Protocol Buffers 定义** (`proto/raft.proto`)
   - 定义了 Raft RPC 消息格式
   - 包含 RequestVote 和 AppendEntries 两种 RPC

2. **RPC 服务端** (`RaftRPCServer`)
   - 监听指定端口
   - 接收并处理 RPC 请求
   - 调用 RaftNode 的处理方法

3. **RPC 客户端** (`RaftRPCClient`)
   - 连接到远程节点
   - 发送 RPC 请求
   - 处理响应和超时

### 消息格式

#### RequestVote RPC
用于候选人请求其他节点投票。

**请求参数**:
- `term`: 候选人的任期号
- `candidate_id`: 候选人的 ID
- `last_log_index`: 候选人最后日志条目的索引
- `last_log_term`: 候选人最后日志条目的任期号

**响应**:
- `term`: 当前任期号
- `vote_granted`: 是否投票给候选人

#### AppendEntries RPC
用于 Leader 复制日志条目和发送心跳。

**请求参数**:
- `term`: Leader 的任期号
- `leader_id`: Leader 的 ID
- `prev_log_index`: 新日志条目之前的日志索引
- `prev_log_term`: prevLogIndex 处日志条目的任期号
- `entries`: 要存储的日志条目（心跳时为空）
- `leader_commit`: Leader 的 commitIndex

**响应**:
- `term`: 当前任期号
- `success`: 是否成功
- `conflict_index`: 冲突日志的索引（用于快速回退优化）
- `conflict_term`: 冲突日志的任期号（用于快速回退优化）

## 使用示例

### 启动 RPC 服务器

```cpp
#include "network/rpc_server.h"
#include "raft/raft_node.h"

// 创建 RaftNode
std::vector<int> peerIds = {2, 3};
auto raftNode = std::make_unique<raft::RaftNode>(1, peerIds);

// 创建并启动 RPC 服务器
auto server = network::createRPCServer(raftNode.get());
if (server->start("127.0.0.1:8001")) {
    std::cout << "RPC Server started successfully" << std::endl;
    server->wait();  // 等待服务器关闭
}
```

### 使用 RPC 客户端

```cpp
#include "network/rpc_client.h"
#include "raft/rpc_messages.h"

// 创建 RPC 客户端
auto client = network::createRPCClient("127.0.0.1:8002");

// 连接到远程节点
if (client->connect("127.0.0.1:8002")) {
    // 发送 RequestVote RPC
    raft::RequestVoteArgs args(1, 1, 0, 0);
    raft::RequestVoteReply reply;
    
    if (client->sendRequestVote(args, reply, 1000)) {
        if (reply.voteGranted) {
            std::cout << "Vote granted!" << std::endl;
        }
    }
    
    client->disconnect();
}
```

### 配置超时和重试

```cpp
auto client = network::createRPCClient("127.0.0.1:8002");

// 设置默认超时时间（毫秒）
client->setDefaultTimeout(2000);

// 设置重试次数
client->setRetries(3);

// 发送请求时指定超时
raft::AppendEntriesArgs args;
raft::AppendEntriesReply reply;
client->sendAppendEntries(args, reply, 500);  // 500ms 超时
```

## Protocol Buffers 语法说明

### 基本语法

```protobuf
syntax = "proto3";  // 使用 proto3 语法

package raft;  // 包名

// 消息定义
message RequestVoteRequest {
    int32 term = 1;           // 字段类型 字段名 = 字段编号
    int32 candidate_id = 2;
    int32 last_log_index = 3;
    int32 last_log_term = 4;
}

// 服务定义
service RaftService {
    rpc RequestVote(RequestVoteRequest) returns (RequestVoteResponse);
}
```

### 数据类型

- `int32`, `int64`: 整数类型
- `bool`: 布尔类型
- `string`: 字符串类型
- `repeated`: 数组类型（如 `repeated LogEntry entries`）

### 编译 .proto 文件

如果要使用 gRPC（可选），需要安装 Protocol Buffers 编译器：

```bash
# 安装 protoc
# Ubuntu/Debian
sudo apt-get install protobuf-compiler

# macOS
brew install protobuf

# 编译 .proto 文件
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` proto/raft.proto
```

## 当前实现：Simple RPC

当前实现使用原生 TCP Socket，不依赖 gRPC。这种实现：

### 优点
- 轻量级，无外部依赖
- 易于理解和调试
- 足够满足 Raft 的需求

### 消息协议

**请求格式**: `METHOD|payload`
- `METHOD`: "RequestVote" 或 "AppendEntries"
- `payload`: 序列化后的参数

**响应格式**: `OK|payload` 或 `ERROR:message`
- 成功: `OK|` + 序列化后的响应
- 失败: `ERROR:` + 错误信息

### 序列化格式

使用简单的文本序列化（在 `rpc_messages.h` 中定义）：

```cpp
// RequestVoteArgs 序列化
"term|candidateId|lastLogIndex|lastLogTerm"

// AppendEntriesArgs 序列化
"term|leaderId|prevLogIndex|prevLogTerm|leaderCommit|entryCount|..."
```

## 升级到 gRPC（可选）

如果需要更强大的功能（如流式 RPC、负载均衡等），可以升级到 gRPC：

### 步骤

1. **安装 gRPC 和 Protocol Buffers**
   ```bash
   # 参考 https://grpc.io/docs/languages/cpp/quickstart/
   ```

2. **编译 .proto 文件**
   ```bash
   protoc --cpp_out=. --grpc_out=. \
          --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` \
          proto/raft.proto
   ```

3. **实现 gRPC 服务端**
   ```cpp
   class RaftServiceImpl final : public raft::RaftService::Service {
       Status RequestVote(ServerContext* context,
                         const RequestVoteRequest* request,
                         RequestVoteResponse* response) override {
           // 实现逻辑
           return Status::OK;
       }
   };
   ```

4. **实现 gRPC 客户端**
   ```cpp
   auto channel = grpc::CreateChannel("localhost:8001",
                                     grpc::InsecureChannelCredentials());
   auto stub = raft::RaftService::NewStub(channel);
   
   RequestVoteRequest request;
   RequestVoteResponse response;
   ClientContext context;
   
   Status status = stub->RequestVote(&context, request, &response);
   ```

5. **更新 CMakeLists.txt**
   ```cmake
   find_package(Protobuf REQUIRED)
   find_package(gRPC REQUIRED)
   
   target_link_libraries(network
       PUBLIC
       raft
       protobuf::libprotobuf
       gRPC::grpc++
   )
   ```

## 常见问题和调试技巧

### 1. 连接失败

**问题**: 客户端无法连接到服务器

**解决方案**:
- 检查服务器是否已启动
- 检查地址和端口是否正确
- 检查防火墙设置
- 使用 `netstat` 或 `ss` 查看端口监听状态

```bash
# Linux/macOS
netstat -an | grep 8001

# Windows
netstat -an | findstr 8001
```

### 2. 超时问题

**问题**: RPC 调用经常超时

**解决方案**:
- 增加超时时间
- 检查网络延迟
- 检查服务器负载
- 使用日志记录请求处理时间

```cpp
// 增加超时时间
client->setDefaultTimeout(5000);  // 5 秒

// 记录处理时间
auto start = std::chrono::steady_clock::now();
client->sendRequestVote(args, reply);
auto end = std::chrono::steady_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
std::cout << "Request took " << duration.count() << "ms" << std::endl;
```

### 3. 序列化错误

**问题**: 消息序列化/反序列化失败

**解决方案**:
- 检查消息格式是否正确
- 确保发送端和接收端使用相同的序列化格式
- 添加错误处理和日志

```cpp
try {
    auto args = raft::RequestVoteArgs::deserialize(payload);
} catch (const std::exception& e) {
    std::cerr << "Deserialization error: " << e.what() << std::endl;
    std::cerr << "Payload: " << payload << std::endl;
}
```

### 4. 调试技巧

**启用详细日志**:
```cpp
// 在 RPC 服务器中添加日志
std::cout << "Received " << method << " request from client" << std::endl;
std::cout << "Payload: " << payload << std::endl;
```

**使用网络抓包工具**:
```bash
# 使用 tcpdump 抓包
sudo tcpdump -i lo -A port 8001

# 使用 Wireshark 分析网络流量
```

**测试连接**:
```bash
# 使用 telnet 测试端口
telnet 127.0.0.1 8001

# 使用 nc (netcat) 测试
echo "RequestVote|1|1|0|0" | nc 127.0.0.1 8001
```

## 性能优化建议

### 1. 连接池

对于频繁通信的节点，维护长连接而不是每次请求都创建新连接：

```cpp
class ConnectionPool {
    std::map<std::string, std::unique_ptr<RaftRPCClient>> clients_;
    
public:
    RaftRPCClient* getClient(const std::string& address) {
        if (clients_.find(address) == clients_.end()) {
            clients_[address] = createRPCClient(address);
            clients_[address]->connect(address);
        }
        return clients_[address].get();
    }
};
```

### 2. 异步 RPC

使用异步 RPC 避免阻塞：

```cpp
// 异步发送请求
std::future<bool> future = std::async(std::launch::async, [&]() {
    return client->sendRequestVote(args, reply);
});

// 继续其他工作...

// 等待结果
if (future.get()) {
    // 处理响应
}
```

### 3. 批量操作

合并多个小请求为一个大请求，减少网络往返：

```cpp
// 在 AppendEntries 中批量发送多个日志条目
std::vector<LogEntry> entries;
for (int i = 0; i < 10; ++i) {
    entries.push_back(getLogEntry(nextIndex + i));
}
args.entries = entries;
```

## 参考资料

- [gRPC 官方文档](https://grpc.io/docs/)
- [Protocol Buffers 文档](https://developers.google.com/protocol-buffers)
- [Raft 论文](https://raft.github.io/raft.pdf)
- [TCP Socket 编程指南](https://beej.us/guide/bgnet/)

## 总结

本 RPC 框架提供了 Raft 节点间通信所需的基本功能：

- ✅ RequestVote RPC（请求投票）
- ✅ AppendEntries RPC（追加日志/心跳）
- ✅ 超时和重试机制
- ✅ 消息序列化
- ✅ 简单易用的接口

当前的 Simple RPC 实现足够满足 Raft 的需求。如果未来需要更高级的功能（如双向流、负载均衡等），可以无缝升级到 gRPC。
