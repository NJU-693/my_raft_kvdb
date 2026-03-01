# API 参考文档

本文档描述 Raft KV Store 的编程接口和客户端 API。

## 目录

- [KVStore API](#kvstore-api)
- [RaftNode API](#raftnode-api)
- [客户端 API](#客户端-api)
- [命令行接口](#命令行接口)

## KVStore API

`KVStore` 类提供基本的键值存储操作。

### 头文件

```cpp
#include "storage/kv_store.h"
```

### 类定义

```cpp
namespace raft_kv {

class KVStore {
public:
    KVStore();
    ~KVStore();
    
    // 基本操作
    void put(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);
    void remove(const std::string& key);
    
    // 命令应用接口
    void applyCommand(const std::string& command);
    std::string applyCommandWithResult(const std::string& command);
};

} // namespace raft_kv
```

### 方法说明

#### put()

插入或更新键值对。

```cpp
void put(const std::string& key, const std::string& value);
```

**参数：**
- `key`: 键（不能包含冒号 `:`）
- `value`: 值（可以包含任意字符）

**示例：**

```cpp
KVStore store;
store.put("user_1", "Alice");
store.put("user_2", "Bob");
```

#### get()

查询键对应的值。

```cpp
bool get(const std::string& key, std::string& value);
```

**参数：**
- `key`: 要查询的键
- `value`: 输出参数，存储查询到的值

**返回值：**
- `true`: 键存在，值已存储到 `value`
- `false`: 键不存在

**示例：**

```cpp
std::string value;
if (store.get("user_1", value)) {
    std::cout << "user_1 = " << value << std::endl;
} else {
    std::cout << "Key not found" << std::endl;
}
```

#### remove()

删除键值对。

```cpp
void remove(const std::string& key);
```

**参数：**
- `key`: 要删除的键

**示例：**

```cpp
store.remove("user_1");
```

#### applyCommand()

应用命令字符串（无返回值）。

```cpp
void applyCommand(const std::string& command);
```

**参数：**
- `command`: 命令字符串，格式为：
  - `PUT:key:value` - 插入/更新
  - `GET:key` - 查询
  - `DELETE:key` - 删除

**示例：**

```cpp
store.applyCommand("PUT:user_1:Alice");
store.applyCommand("DELETE:user_2");
```

#### applyCommandWithResult()

应用命令字符串并返回结果。

```cpp
std::string applyCommandWithResult(const std::string& command);
```

**参数：**
- `command`: 命令字符串（格式同上）

**返回值：**
- `"OK"`: 操作成功（PUT/DELETE）
- `value`: 查询到的值（GET）
- `"NOT_FOUND"`: 键不存在（GET）

**示例：**

```cpp
std::string result = store.applyCommandWithResult("GET:user_1");
if (result != "NOT_FOUND") {
    std::cout << "Value: " << result << std::endl;
}
```

## RaftNode API

`RaftNode` 类实现 Raft 共识算法。

### 头文件

```cpp
#include "raft/raft_node.h"
```

### 主要方法

#### submitCommand()

提交命令到 Raft 集群。

```cpp
std::pair<bool, std::string> submitCommand(const std::string& command);
```

**参数：**
- `command`: 要提交的命令字符串

**返回值：**
- `pair.first`: 是否成功
- `pair.second`: 结果字符串或错误信息

**示例：**

```cpp
auto [success, result] = raft_node->submitCommand("PUT:key1:value1");
if (success) {
    std::cout << "Command applied: " << result << std::endl;
} else {
    std::cerr << "Command failed: " << result << std::endl;
}
```

#### isLeader()

检查当前节点是否为 Leader。

```cpp
bool isLeader() const;
```

**返回值：**
- `true`: 当前节点是 Leader
- `false`: 当前节点不是 Leader

#### getLeaderId()

获取当前 Leader 的 ID。

```cpp
int getLeaderId() const;
```

**返回值：**
- Leader 的节点 ID
- `-1`: 当前没有 Leader

#### getCurrentTerm()

获取当前任期号。

```cpp
int getCurrentTerm() const;
```

**返回值：**
- 当前任期号

## 客户端 API

`KVClient` 类提供客户端接口。

### 头文件

```cpp
#include "client/kv_client.h"
```

### 类定义

```cpp
namespace raft_kv {

class KVClient {
public:
    KVClient(const std::vector<std::string>& server_addresses);
    ~KVClient();
    
    bool put(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);
    bool remove(const std::string& key);
};

} // namespace raft_kv
```

### 方法说明

#### 构造函数

```cpp
KVClient(const std::vector<std::string>& server_addresses);
```

**参数：**
- `server_addresses`: 服务器地址列表（格式：`"host:port"`）

**示例：**

```cpp
std::vector<std::string> servers = {
    "localhost:8001",
    "localhost:8002",
    "localhost:8003"
};
KVClient client(servers);
```

#### put()

插入或更新键值对。

```cpp
bool put(const std::string& key, const std::string& value);
```

**返回值：**
- `true`: 操作成功
- `false`: 操作失败

**示例：**

```cpp
if (client.put("key1", "value1")) {
    std::cout << "PUT successful" << std::endl;
}
```

#### get()

查询键对应的值。

```cpp
bool get(const std::string& key, std::string& value);
```

**返回值：**
- `true`: 键存在
- `false`: 键不存在或查询失败

**示例：**

```cpp
std::string value;
if (client.get("key1", value)) {
    std::cout << "Value: " << value << std::endl;
}
```

#### remove()

删除键值对。

```cpp
bool remove(const std::string& key);
```

**返回值：**
- `true`: 操作成功
- `false`: 操作失败

**示例：**

```cpp
if (client.remove("key1")) {
    std::cout << "DELETE successful" << std::endl;
}
```

## 命令行接口

### 交互式模式

```bash
./build/bin/kv_client_cli
```

**可用命令：**

| 命令 | 格式 | 说明 | 示例 |
|------|------|------|------|
| put | `put <key> <value>` | 插入/更新键值对 | `put user_1 Alice` |
| get | `get <key>` | 查询键的值 | `get user_1` |
| delete | `delete <key>` | 删除键值对 | `delete user_1` |
| help | `help` | 显示帮助信息 | `help` |
| quit | `quit` 或 `exit` | 退出客户端 | `quit` |

**示例会话：**

```
$ ./build/bin/kv_client_cli
Welcome to Raft KV Client!
Type 'help' for available commands.

raft-kv> put name Alice
OK

raft-kv> get name
Alice

raft-kv> delete name
OK

raft-kv> quit
Goodbye!
```

### 非交互式模式

```bash
./build/bin/kv_client_cli <command> <key> [value]
```

**示例：**

```bash
# PUT 操作
./build/bin/kv_client_cli PUT key1 value1

# GET 操作
./build/bin/kv_client_cli GET key1

# DELETE 操作
./build/bin/kv_client_cli DELETE key1
```

**在脚本中使用：**

```bash
#!/bin/bash

CLIENT="./build/bin/kv_client_cli"

# 写入数据
$CLIENT PUT user_1 Alice
$CLIENT PUT user_2 Bob

# 读取数据
VALUE=$($CLIENT GET user_1)
echo "user_1 = $VALUE"

# 删除数据
$CLIENT DELETE user_2
```

## 错误处理

### 常见错误

| 错误信息 | 原因 | 解决方案 |
|---------|------|---------|
| `NOT_LEADER` | 当前节点不是 Leader | 客户端会自动重定向到 Leader |
| `NOT_FOUND` | 键不存在 | 检查键名是否正确 |
| `Max retries exceeded` | 无法连接到集群 | 检查集群是否运行 |
| `Invalid command format` | 命令格式错误 | 检查命令格式 |
| `Key cannot be empty` | 键为空 | 提供有效的键 |

### 重试机制

客户端自动实现重试机制：
- 最大重试次数：3 次
- 重试间隔：1 秒
- 自动 Leader 重定向

## 性能考虑

### 批量操作

对于大量操作，建议：

```cpp
// 不推荐：逐个操作
for (int i = 0; i < 1000; i++) {
    client.put("key" + std::to_string(i), "value" + std::to_string(i));
}

// 推荐：使用并发客户端
std::vector<std::thread> threads;
for (int t = 0; t < 5; t++) {
    threads.emplace_back([&, t]() {
        KVClient client(servers);
        for (int i = t * 200; i < (t + 1) * 200; i++) {
            client.put("key" + std::to_string(i), "value" + std::to_string(i));
        }
    });
}
for (auto& thread : threads) {
    thread.join();
}
```

### 连接复用

```cpp
// 推荐：复用客户端实例
KVClient client(servers);
for (int i = 0; i < 1000; i++) {
    client.put("key" + std::to_string(i), "value" + std::to_string(i));
}

// 不推荐：每次创建新客户端
for (int i = 0; i < 1000; i++) {
    KVClient client(servers);  // 开销大
    client.put("key" + std::to_string(i), "value" + std::to_string(i));
}
```

## 线程安全

- `KVStore`: 线程安全（内部使用互斥锁）
- `RaftNode`: 线程安全（内部使用互斥锁）
- `KVClient`: 非线程安全（每个线程应使用独立的客户端实例）

## 示例代码

完整示例请参考 `examples/` 目录：
- `examples/rpc_demo.cpp` - RPC 框架使用示例
- `examples/nextindex_update_demo.cpp` - nextIndex 更新机制
- `examples/conflict_resolution_demo.cpp` - 日志冲突解决

## 相关文档

- [部署指南](deployment-guide.md)
- [架构设计](architecture.md)
- [性能测试](performance-test-results.md)
