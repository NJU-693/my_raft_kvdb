# RPC 框架使用指南

## 概述

本项目使用 **gRPC** 作为 Raft 节点之间的 RPC（远程过程调用）框架。gRPC 是一个高性能、开源的通用 RPC 框架，基于 HTTP/2 协议，使用 Protocol Buffers 作为接口定义语言（IDL）和消息序列化格式。

> **注意**: 项目已从 Simple RPC（基于 TCP Socket）升级到 gRPC。如需了解 gRPC 的详细使用说明，请参考 [docs/grpc-guide.md](grpc-guide.md)。

## 架构设计

### 核心组件

1. **Protocol Buffers 定义** (`proto/raft.proto`)
   - 定义了 Raft RPC 消息格式和服务接口
   - 包含 RequestVote 和 AppendEntries 两种 RPC
   - 使用 protoc 编译生成 C++ 代码

2. **RPC 服务端** (`GrpcRPCServer`)
   - 继承 `RaftRPCServer` 接口和 `raft::RaftService::Service`
   - 监听指定端口，接收并处理 gRPC 请求
   - 调用 RaftNode 的处理方法
   - 实现 proto 消息和 C++ 对象之间的转换

3. **RPC 客户端** (`GrpcRPCClient`)
   - 继承 `RaftRPCClient` 接口
   - 连接到远程节点，发送 gRPC 请求
   - 处理响应、超时和重试
   - 实现 C++ 对象和 proto 消息之间的转换

4. **工厂函数** (`rpc_factory.cpp`)
   - `createRPCServer()`: 创建 gRPC 服务器实例
   - `createRPCClient()`: 创建 gRPC 客户端实例

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

### 启动 gRPC 服务器

```cpp
#include "network/rpc_server.h"
#include "raft/raft_node.h"

// 创建 RaftNode
std::vector<int> peerIds = {2, 3};
auto raftNode = std::make_unique<raft::RaftNode>(1, peerIds);

// 创建并启动 gRPC 服务器
auto server = network::createRPCServer(raftNode.get());
if (server->start("0.0.0.0:50051")) {
    std::cout << "gRPC Server started successfully" << std::endl;
    server->wait();  // 等待服务器关闭
}
```

### 使用 gRPC 客户端

```cpp
#include "network/rpc_client.h"
#include "raft/rpc_messages.h"

// 创建 gRPC 客户端（自动连接）
auto client = network::createRPCClient("localhost:50051");

// 发送 RequestVote RPC
raft::RequestVoteArgs args(1, 1, 0, 0);
raft::RequestVoteReply reply;

if (client->sendRequestVote(args, reply, 1000)) {
    if (reply.voteGranted) {
        std::cout << "Vote granted!" << std::endl;
    }
} else {
    std::cerr << "RPC failed" << std::endl;
}
```
```

### 配置超时和重试

```cpp
auto client = network::createRPCClient("localhost:50051");

// 设置默认超时时间（毫秒）
client->setDefaultTimeout(2000);

// 设置重试次数
client->setRetries(3);

// 发送请求时指定超时
raft::AppendEntriesArgs args;
raft::AppendEntriesReply reply;
client->sendAppendEntries(args, reply, 500);  // 500ms 超时
```

## gRPC 实现优势

相比之前的 Simple RPC 实现，gRPC 提供了以下优势：

### 1. 性能优势
- **HTTP/2 协议**: 支持多路复用、头部压缩、服务器推送
- **高效序列化**: Protocol Buffers 比 JSON/XML 更快、更小
- **连接复用**: 自动管理连接池，减少连接开销

### 2. 可靠性
- **成熟稳定**: 经过 Google 和众多公司的生产环境验证
- **错误处理**: 完善的错误码和状态管理
- **超时控制**: 精确的超时和截止时间控制

### 3. 可扩展性
- **跨语言支持**: 支持多种编程语言，便于未来扩展
- **丰富的工具**: 支持 grpcurl、grpc-gateway 等工具
- **流式 RPC**: 支持双向流（虽然当前未使用）

### 4. 开发效率
- **自动代码生成**: 从 .proto 文件自动生成客户端和服务端代码
- **类型安全**: 编译时类型检查，减少运行时错误
- **文档化**: .proto 文件本身就是接口文档

## Protocol Buffers 使用说明

### 编译 .proto 文件

当修改 `proto/raft.proto` 后，需要重新编译生成 C++ 代码：

```bash
cd proto
protoc --cpp_out=. --grpc_out=. \
       --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` \
       raft.proto
```

这将生成以下文件：
- `raft.pb.h` / `raft.pb.cc`: Protocol Buffers 消息定义
- `raft.grpc.pb.h` / `raft.grpc.pb.cc`: gRPC 服务定义

### Proto 文件语法

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

## 构建配置

### CMakeLists.txt 配置

项目已配置好 gRPC 和 Protocol Buffers 的构建：

```cmake
# 查找 Protobuf
find_package(Protobuf REQUIRED)

# 查找 gRPC（使用 pkg-config）
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)

# 添加 proto 生成的源文件库
add_library(raft_proto
    proto/raft.pb.cc
    proto/raft.grpc.pb.cc
)
target_link_libraries(raft_proto PUBLIC
    ${PROTOBUF_LIBRARIES}
    ${GRPC_LIBRARIES}
)
```

### 安装依赖

**Ubuntu/Debian**:
```bash
sudo apt install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
```

**从源码编译** (如果系统包不可用):
```bash
git clone --recurse-submodules -b v1.50.0 https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build && cd cmake/build
cmake ../..
make -j4
sudo make install
```

## 常见问题和调试技巧

### 1. 连接失败

**问题**: 客户端无法连接到服务器

**解决方案**:
- 检查服务器是否已启动
- 检查地址和端口是否正确（gRPC 默认使用 50051 端口）
- 检查防火墙设置
- 使用 `netstat` 或 `ss` 查看端口监听状态

```bash
# Linux/macOS
netstat -an | grep 50051
ss -tuln | grep 50051

# 或使用 lsof
lsof -i :50051
```

### 2. 超时问题

**问题**: RPC 调用经常超时

**解决方案**:
- 增加超时时间
- 检查网络延迟
- 检查服务器负载
- 启用 gRPC 日志查看详细信息

```bash
# 启用 gRPC 详细日志
export GRPC_VERBOSITY=DEBUG
export GRPC_TRACE=all
```

### 3. 编译错误

**问题**: 找不到 gRPC 或 Protobuf 头文件

**解决方案**:
```bash
# 检查是否安装了开发包
dpkg -l | grep grpc
dpkg -l | grep protobuf

# 重新安装
sudo apt install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
```

### 4. 调试技巧

**使用 grpcurl 测试服务**:
```bash
# 安装 grpcurl
go install github.com/fullstorydev/grpcurl/cmd/grpcurl@latest

# 列出服务
grpcurl -plaintext localhost:50051 list

# 调用方法
grpcurl -plaintext -d '{"term": 1, "candidate_id": 2}' \
    localhost:50051 raft.RaftService/RequestVote
```

**查看 gRPC 流量**:
```bash
# 使用 tcpdump 抓包
sudo tcpdump -i lo -w grpc.pcap port 50051

# 使用 Wireshark 分析（HTTP/2 协议）
```

## 性能优化建议

### 1. 连接复用

gRPC 自动管理连接池和连接复用，无需手动管理：

```cpp
// gRPC 客户端会自动复用连接
auto client = network::createRPCClient("localhost:50051");

// 多次调用会复用同一个连接
for (int i = 0; i < 100; ++i) {
    client->sendRequestVote(args, reply);
}
```

### 2. 异步 RPC（未来扩展）

当前实现使用同步 RPC。如需更高性能，可以实现异步版本：

```cpp
// 未来可能的异步接口
std::future<RequestVoteReply> sendRequestVoteAsync(const RequestVoteArgs& args);
```

### 3. 批量操作

合并多个日志条目为一个 AppendEntries 请求：

```cpp
// 批量发送多个日志条目
std::vector<LogEntry> entries;
for (int i = 0; i < 10; ++i) {
    entries.push_back(getLogEntry(nextIndex + i));
}
args.entries = entries;
client->sendAppendEntries(args, reply);
```

### 4. 调整 gRPC 参数

```cpp
// 在创建 channel 时设置参数
grpc::ChannelArguments args;
args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 10000);
args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000);
args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);

auto channel = grpc::CreateCustomChannel(
    address, 
    grpc::InsecureChannelCredentials(), 
    args
);
```

## 参考资料

- [gRPC 官方文档](https://grpc.io/docs/)
- [gRPC C++ 快速开始](https://grpc.io/docs/languages/cpp/quickstart/)
- [Protocol Buffers 文档](https://developers.google.com/protocol-buffers)
- [Raft 论文](https://raft.github.io/raft.pdf)
- [gRPC 性能最佳实践](https://grpc.io/docs/guides/performance/)

## 总结

本项目使用 gRPC 作为 RPC 框架，提供了以下功能：

- ✅ RequestVote RPC（请求投票）
- ✅ AppendEntries RPC（追加日志/心跳）
- ✅ 超时和重试机制
- ✅ 高效的 Protocol Buffers 序列化
- ✅ HTTP/2 协议支持
- ✅ 自动连接管理和复用
- ✅ 完善的错误处理
- ✅ 跨平台支持

gRPC 提供了生产级别的性能和可靠性，是构建分布式系统的理想选择。

## 附录：Simple RPC 实现

项目中仍保留了 Simple RPC 实现（`simple_rpc_server.cpp` 和 `simple_rpc_client.cpp`）作为参考。如需切换回 Simple RPC，可以修改 `rpc_factory.cpp` 中的工厂函数。

Simple RPC 的优点：
- 轻量级，无外部依赖
- 易于理解和调试
- 适合学习和原型开发

gRPC 的优点：
- 生产级别的性能和可靠性
- 丰富的工具和生态系统
- 跨语言支持
- 更好的错误处理和超时控制
