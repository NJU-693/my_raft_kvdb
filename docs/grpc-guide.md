# gRPC 使用指南

## 概述

本项目使用 gRPC 作为 Raft 节点间的 RPC 通信框架。gRPC 是一个高性能、开源的通用 RPC 框架，基于 HTTP/2 协议，使用 Protocol Buffers 作为接口定义语言（IDL）和消息序列化格式。

## 架构设计

### 1. Proto 定义

位置：`proto/raft.proto`

定义了以下消息和服务：

**消息类型**：
- `LogEntryProto`：日志条目
- `RequestVoteRequest/Response`：投票请求和响应
- `AppendEntriesRequest/Response`：日志追加请求和响应

**服务接口**：
- `RaftService`：包含 `RequestVote` 和 `AppendEntries` 两个 RPC 方法

### 2. 实现类

#### GrpcRPCServer
- 继承 `RaftRPCServer` 接口和 `raft::RaftService::Service`
- 实现 gRPC 服务端逻辑
- 负责接收 RPC 请求并调用 `RaftNode` 处理

**关键方法**：
- `start(address)`：启动 gRPC 服务器
- `stop()`：停止服务器
- `RequestVote()`：处理投票请求
- `AppendEntries()`：处理日志追加请求

#### GrpcRPCClient
- 继承 `RaftRPCClient` 接口
- 实现 gRPC 客户端逻辑
- 负责向其他节点发送 RPC 请求

**关键方法**：
- `connect(address)`：连接到远程节点
- `sendRequestVote()`：发送投票请求
- `sendAppendEntries()`：发送日志追加请求
- `setDefaultTimeout()`：设置默认超时时间
- `setRetries()`：设置重试次数

### 3. 消息转换

由于 gRPC 使用 Protocol Buffers 消息格式，而 Raft 核心模块使用 C++ 对象，因此需要在两者之间进行转换：

**服务端转换**：
- `protoToRequestVoteArgs()`：proto → C++ 对象
- `requestVoteReplyToProto()`：C++ 对象 → proto
- `protoToAppendEntriesArgs()`：proto → C++ 对象
- `appendEntriesReplyToProto()`：C++ 对象 → proto

**客户端转换**：
- `requestVoteArgsToProto()`：C++ 对象 → proto
- `protoToRequestVoteReply()`：proto → C++ 对象
- `appendEntriesArgsToProto()`：C++ 对象 → proto
- `protoToAppendEntriesReply()`：proto → C++ 对象

## 使用方法

### 1. 编译 Proto 文件

如果修改了 `proto/raft.proto`，需要重新编译：

```bash
cd proto
protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` raft.proto
```

这将生成以下文件：
- `raft.pb.h` / `raft.pb.cc`：Protocol Buffers 消息定义
- `raft.grpc.pb.h` / `raft.grpc.pb.cc`：gRPC 服务定义

### 2. 启动 gRPC 服务器

```cpp
#include "network/rpc_server.h"
#include "raft/raft_node.h"

// 创建 RaftNode
auto raft_node = std::make_unique<raft::RaftNode>(/* 参数 */);

// 创建并启动 gRPC 服务器
auto server = network::createRPCServer(raft_node.get());
if (!server->start("0.0.0.0:50051")) {
    std::cerr << "Failed to start server" << std::endl;
    return 1;
}

// 等待服务器关闭
server->wait();
```

### 3. 使用 gRPC 客户端

```cpp
#include "network/rpc_client.h"
#include "raft/rpc_messages.h"

// 创建客户端
auto client = network::createRPCClient("localhost:50051");

// 发送 RequestVote RPC
raft::RequestVoteArgs args;
args.term = 1;
args.candidateId = 2;
args.lastLogIndex = 0;
args.lastLogTerm = 0;

raft::RequestVoteReply reply;
if (client->sendRequestVote(args, reply, 1000)) {
    std::cout << "Vote granted: " << reply.voteGranted << std::endl;
} else {
    std::cerr << "RPC failed" << std::endl;
}
```

## 配置选项

### 超时设置

gRPC 客户端支持配置超时时间：

```cpp
client->setDefaultTimeout(2000);  // 设置默认超时为 2 秒
```

也可以在每次 RPC 调用时指定超时：

```cpp
client->sendRequestVote(args, reply, 500);  // 超时 500ms
```

### 重试机制

可以配置 RPC 失败时的重试次数：

```cpp
client->setRetries(3);  // 失败后重试 3 次
```

### 服务器地址格式

地址格式为 `host:port`，例如：
- `0.0.0.0:50051`：监听所有网络接口的 50051 端口
- `127.0.0.1:50051`：仅监听本地回环接口
- `192.168.1.100:50051`：监听特定 IP 地址

## 性能优化

### 1. 连接复用

gRPC 客户端会自动复用 TCP 连接，无需为每次 RPC 调用创建新连接。

### 2. 异步调用（未实现）

当前实现使用同步 RPC 调用。如需更高性能，可以实现异步版本：

```cpp
// 未来可能的异步接口
std::future<RequestVoteReply> sendRequestVoteAsync(const RequestVoteArgs& args);
```

### 3. 批量操作（未实现）

对于日志复制，可以考虑批量发送多个日志条目以减少 RPC 次数。

## 错误处理

### 常见错误

1. **连接失败**：
   - 检查目标节点是否启动
   - 检查网络连接和防火墙设置
   - 验证地址格式是否正确

2. **超时**：
   - 增加超时时间
   - 检查网络延迟
   - 检查服务端是否过载

3. **序列化错误**：
   - 确保 proto 文件版本一致
   - 检查消息字段是否正确设置

### 错误日志

gRPC 实现会输出详细的错误日志到 `std::cerr`，包括：
- RPC 调用失败的原因
- 网络连接状态
- 超时信息

## 调试技巧

### 1. 启用 gRPC 日志

设置环境变量启用详细日志：

```bash
export GRPC_VERBOSITY=DEBUG
export GRPC_TRACE=all
```

### 2. 使用 grpcurl 测试

安装 grpcurl 工具测试 gRPC 服务：

```bash
# 列出服务
grpcurl -plaintext localhost:50051 list

# 调用方法
grpcurl -plaintext -d '{"term": 1, "candidate_id": 2}' \
    localhost:50051 raft.RaftService/RequestVote
```

### 3. 网络抓包

使用 Wireshark 或 tcpdump 抓取 gRPC 流量（HTTP/2）：

```bash
tcpdump -i lo -w grpc.pcap port 50051
```

## 与 Simple RPC 的对比

| 特性 | Simple RPC | gRPC |
|------|-----------|------|
| 协议 | TCP Socket | HTTP/2 |
| 序列化 | 自定义 | Protocol Buffers |
| 性能 | 中等 | 高 |
| 跨语言 | 否 | 是 |
| 工具支持 | 少 | 丰富 |
| 学习曲线 | 低 | 中等 |
| 生产就绪 | 否 | 是 |

## 未来扩展

### 1. SSL/TLS 支持

添加加密通信支持：

```cpp
auto creds = grpc::SslServerCredentials(ssl_opts);
builder.AddListeningPort(address, creds);
```

### 2. 认证和授权

实现节点间的身份验证：

```cpp
class AuthInterceptor : public grpc::ServerInterceptor {
    // 实现认证逻辑
};
```

### 3. 负载均衡

使用 gRPC 的负载均衡功能：

```cpp
grpc::ChannelArguments args;
args.SetLoadBalancingPolicyName("round_robin");
```

### 4. 健康检查

实现 gRPC 健康检查协议：

```cpp
class HealthServiceImpl : public grpc::health::v1::Health::Service {
    // 实现健康检查
};
```

## 参考资料

- [gRPC 官方文档](https://grpc.io/docs/)
- [Protocol Buffers 文档](https://developers.google.com/protocol-buffers)
- [gRPC C++ 快速开始](https://grpc.io/docs/languages/cpp/quickstart/)
- [Raft 论文](https://raft.github.io/raft.pdf)

## 故障排查

### 问题：找不到 gRPC 库

**解决方案**：
```bash
# Ubuntu/Debian
sudo apt install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc

# 或从源码编译
git clone --recurse-submodules -b v1.50.0 https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
cd cmake/build
cmake ../..
make -j4
sudo make install
```

### 问题：CMake 找不到 gRPC

**解决方案**：
```cmake
# 在 CMakeLists.txt 中指定路径
set(CMAKE_PREFIX_PATH "/usr/local" ${CMAKE_PREFIX_PATH})
find_package(gRPC CONFIG REQUIRED)
```

### 问题：链接错误

**解决方案**：
确保链接了所有必要的库：
```cmake
target_link_libraries(your_target
    raft_proto
    gRPC::grpc++
    gRPC::grpc++_reflection
    protobuf::libprotobuf
)
```
