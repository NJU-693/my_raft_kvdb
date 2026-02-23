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
- ✅ **客户端接口**：提供简单的 Put/Get/Delete 操作

## 项目结构

```
raft-kv-store/
├── CMakeLists.txt          # 主构建配置
├── README.md               # 项目说明
├── include/                # 头文件
│   ├── raft/              # Raft 算法相关
│   ├── storage/           # 存储引擎相关
│   ├── network/           # 网络通信相关
│   └── util/              # 工具类
├── src/                    # 源文件
│   ├── raft/
│   ├── storage/
│   ├── network/
│   └── util/
├── test/                   # 测试文件
├── examples/               # 示例程序
├── docs/                   # 详细文档
└── config/                 # 配置文件
```

## 快速开始

### 环境要求

- C++17 或更高版本
- CMake 3.14+
- 支持的编译器：GCC 7+, Clang 6+, MSVC 2017+

### 编译

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 编译
cmake --build .

# 运行测试（可选）
ctest
```

### 运行示例

```bash
# 单节点模式
./bin/single_node

# 集群模式（需要先配置 config/cluster.conf）
./bin/cluster --config ../config/cluster.conf --node-id 1
```

## 使用示例

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

## 文档

详细文档请查看 `docs/` 目录：
- [架构设计](docs/architecture.md)
- [Raft 算法详解](docs/raft-algorithm.md)
- [跳表实现说明](docs/skiplist.md)

## 开发进度

- [x] 项目初始化
- [ ] 跳表数据结构实现
- [ ] KV 存储引擎实现
- [ ] Raft 核心算法实现
- [ ] 网络通信实现
- [ ] 集成测试

详细任务列表请查看 `.kiro/specs/raft-kv-store/tasks.md`

## 学习资源

- [Raft 论文](https://raft.github.io/raft.pdf)
- [Raft 可视化演示](https://raft.github.io/)
- [MIT 6.824 分布式系统课程](https://pdos.csail.mit.edu/6.824/)

## 许可证

本项目仅用于学习目的。

## 作者

个人学习项目
