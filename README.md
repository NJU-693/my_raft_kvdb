# Raft KV 分布式存储系统

一个基于 Raft 共识算法的分布式键值存储系统，使用 C++ 实现。

## 项目简介

本项目是一个学习型分布式系统项目，旨在深入理解：
- 分布式系统的基本原理
- Raft 共识算法的实现细节
- 跳表数据结构及其在 KV 存储中的应用
- C++ 系统编程和网络编程

## 核心特性

- ✅ **Raft 共识算法**：实现领导者选举、日志复制和安全性保证
- ✅ **跳表存储引擎**：高效的 O(log n) 查找、插入和删除操作
- ✅ **分布式一致性**：保证多节点数据一致性
- ✅ **持久化存储**：支持节点重启后数据恢复
- ✅ **RPC 通信框架**：支持 gRPC 和简单 RPC 两种实现
- ✅ **客户端接口**：提供简单的 Put/Get/Delete 操作
- ✅ **集群管理**：提供启动、停止、重启脚本
- ✅ **命令行工具**：交互式客户端命令行界面

## 项目结构

```
raft-kv-store/
├── CMakeLists.txt          # 主构建配置
├── README.md               # 项目说明
├── include/                # 头文件
│   ├── raft/              # Raft 算法相关
│   ├── storage/           # 存储引擎相关
│   ├── network/           # 网络通信相关
│   ├── client/            # 客户端接口
│   └── util/              # 工具类
├── src/                    # 源文件
│   ├── raft/
│   ├── storage/
│   ├── network/
│   ├── client/
│   ├── util/
│   ├── raft_node_server.cpp    # 节点服务器主程序
│   └── kv_client_cli.cpp       # 客户端命令行工具
├── test/                   # 测试文件
├── examples/               # 示例程序
├── docs/                   # 详细文档
├── config/                 # 配置文件
│   └── cluster.conf       # 集群配置
├── scripts/                # 管理脚本
│   ├── start_cluster.sh   # 启动集群
│   ├── stop_cluster.sh    # 停止集群
│   ├── restart_cluster.sh # 重启集群
│   └── check_cluster.sh   # 检查集群状态
├── data/                   # 持久化数据目录（运行时创建）
└── logs/                   # 日志目录（运行时创建）
```

## 快速开始

> **🎉 任务 9.1 已完成！** 系统集成工作已完成，详见 [TASK_9.1_COMPLETE.md](TASK_9.1_COMPLETE.md)。
> 
> **⚠️ 已知问题**：GET 请求可能超时，但 PUT/DELETE 正常工作。详见 [CURRENT_STATUS.md](CURRENT_STATUS.md)。
> 
> **编译说明**：如果遇到编译问题，请参考 [COMPILE_FIX.md](COMPILE_FIX.md)。

### 环境要求

- C++17 或更高版本
- CMake 3.14+
- gRPC 和 Protocol Buffers
- pthread 库
- 支持的编译器：GCC 7+, Clang 6+, MSVC 2017+

详细的编译指南请参考 [BUILD.md](BUILD.md)。

### 编译

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译
make -j$(nproc)

# 运行测试（可选）
ctest
```

### 启动集群

```bash
# 使用启动脚本启动 3 节点集群
chmod +x scripts/*.sh
./scripts/start_cluster.sh

# 检查集群状态
./scripts/check_cluster.sh

# 查看节点日志
tail -f logs/node1.log
```

### 使用客户端

```bash
# 启动客户端命令行工具
./build/bin/kv_client_cli

# 在客户端中执行操作
raft-kv> put key1 value1
OK
raft-kv> get key1
value1
raft-kv> delete key1
OK
raft-kv> quit
```

### 停止集群

```bash
./scripts/stop_cluster.sh
```

### 运行示例

```bash
# 编译示例程序
cd build
cmake .. -DBUILD_EXAMPLES=ON
make

# RPC 框架演示
./bin/rpc_demo

# nextIndex 更新机制演示
./bin/nextindex_update_demo

# 日志冲突解决演示
./bin/conflict_resolution_demo
```

## 使用示例

### 编程接口

```cpp
// 创建 KV 存储实例
KVStore store;

// 插入数据
store.put("key1", "value1");

// 查询数据
std::string value;
if (store.get("key1", value)) {
    std::cout << "key1 = " << value << std::endl;
}

// 删除数据
store.remove("key1");
```

### 客户端命令行

```bash
# 启动客户端
./build/bin/kv_client_cli

# 执行操作
raft-kv> put user:1 Alice
OK
raft-kv> put user:2 Bob
OK
raft-kv> get user:1
Alice
raft-kv> delete user:2
OK
```

## 文档

详细文档请查看 `docs/` 目录：
- [部署指南](docs/deployment-guide.md) - 如何编译、部署和运行集群
- [架构设计](docs/architecture.md) - 系统架构和设计决策
- [Raft 算法详解](docs/raft-algorithm.md) - Raft 共识算法实现细节
- [跳表实现说明](docs/skiplist.md) - 跳表数据结构详解
- [RPC 框架使用指南](docs/rpc-framework.md) - 网络通信框架说明
- [示例程序指南](examples/README.md) - 演示程序使用说明

## 开发进度

- [x] 阶段一：基础框架搭建
  - [x] 项目初始化
  - [x] 跳表数据结构实现
  - [x] KV 存储引擎实现
- [x] 阶段二：Raft 核心实现
  - [x] Raft 基础组件
  - [x] 领导者选举
  - [x] 日志复制
  - [x] 持久化实现
- [x] 阶段三：网络通信实现
  - [x] RPC 框架（gRPC + Simple RPC）
  - [x] 客户端接口
- [x] 阶段四：系统集成
  - [x] Raft 与 KV 存储集成
  - [x] 节点启动程序
  - [x] 集群管理脚本
  - [x] 客户端命令行工具
  - [ ] 集成测试
  - [ ] 文档完善
- [ ] 阶段五：优化与扩展（可选）
  - [ ] 性能优化
  - [ ] 日志压缩（Snapshot）
  - [ ] 只读优化
  - [ ] 集群成员变更

详细任务列表请查看 `.kiro/specs/raft-kv-store/tasks.md`

## 学习资源

- [Raft 论文](https://raft.github.io/raft.pdf)
- [Raft 可视化演示](https://raft.github.io/)
- [MIT 6.824 分布式系统课程](https://pdos.csail.mit.edu/6.824/)

## 许可证

本项目仅用于学习目的。

## 作者

个人学习项目
