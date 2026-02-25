# 快速开始指南

5 分钟快速体验 Raft KV 分布式存储系统。

## 前置条件

确保已安装：
- C++17 编译器
- CMake 3.14+
- gRPC 和 Protocol Buffers

## 步骤 1：编译项目

```bash
# 克隆或进入项目目录
cd /path/to/raft-kv-store

# 创建并进入构建目录
mkdir -p build && cd build

# 配置和编译
cmake .. && make -j$(nproc)
```

编译成功后，可执行文件位于 `build/bin/` 目录。

## 步骤 2：启动集群

```bash
# 返回项目根目录
cd ..

# 给脚本添加执行权限（仅首次需要）
chmod +x scripts/*.sh

# 启动 3 节点集群
./scripts/start_cluster.sh
```

你应该看到类似输出：

```
==========================================
  Starting Raft KV Cluster
==========================================
Node Count: 3
Config File: /path/to/config/cluster.conf

Starting nodes...
  Starting node 1 (log: /path/to/logs/node1.log)
  Starting node 2 (log: /path/to/logs/node2.log)
  Starting node 3 (log: /path/to/logs/node3.log)

==========================================
  Cluster started successfully!
==========================================

Node status:
  Node 1: PID 12345
  Node 2: PID 12346
  Node 3: PID 12347
```

## 步骤 3：检查集群状态

```bash
# 检查所有节点是否正常运行
./scripts/check_cluster.sh
```

输出示例：

```
==========================================
  Raft KV Cluster Status
==========================================

✓ node1 (PID: 12345) - RUNNING
✓ node2 (PID: 12346) - RUNNING
✓ node3 (PID: 12347) - RUNNING

Summary: 3/3 nodes running
```

## 步骤 4：使用客户端

```bash
# 启动客户端命令行工具
./build/bin/kv_client_cli
```

在客户端中执行操作：

```
raft-kv> put name Alice
OK

raft-kv> put age 25
OK

raft-kv> get name
Alice

raft-kv> get age
25

raft-kv> put name Bob
OK

raft-kv> get name
Bob

raft-kv> delete age
OK

raft-kv> get age
Error: Key not found

raft-kv> quit
```

## 步骤 5：查看日志（可选）

在另一个终端窗口中：

```bash
# 实时查看节点 1 的日志
tail -f logs/node1.log

# 或查看所有节点日志
tail -f logs/node*.log
```

你会看到：
- 选举过程
- Leader 心跳
- 日志复制
- 客户端请求处理

## 步骤 6：停止集群

```bash
# 停止所有节点
./scripts/stop_cluster.sh
```

输出示例：

```
==========================================
  Stopping Raft KV Cluster
==========================================

Stopping node1 (PID: 12345)...
  node1 stopped
Stopping node2 (PID: 12346)...
  node2 stopped
Stopping node3 (PID: 12347)...
  node3 stopped

==========================================
  Cluster stopped successfully!
==========================================
```

## 常见问题

### Q: 编译失败，提示找不到 gRPC

A: 安装 gRPC 和 Protocol Buffers：

```bash
# Ubuntu/Debian
sudo apt-get install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc

# macOS
brew install grpc protobuf
```

### Q: 节点启动失败，端口被占用

A: 修改 `config/cluster.conf` 中的端口号，或停止占用端口的进程：

```bash
# 查看端口占用
netstat -tuln | grep 800[1-3]

# 或使用 lsof
lsof -i :8001
```

### Q: 客户端无法连接

A: 确保集群已启动并运行：

```bash
./scripts/check_cluster.sh
```

### Q: 如何清理数据重新开始

A: 删除数据和日志目录：

```bash
rm -rf data/ logs/
```

## 下一步

- 阅读 [部署指南](docs/deployment-guide.md) 了解详细配置
- 阅读 [架构文档](docs/architecture.md) 了解系统设计
- 阅读 [Raft 算法说明](docs/raft-algorithm.md) 了解共识算法
- 运行 [示例程序](examples/README.md) 学习核心机制

## 测试故障恢复

想看看 Raft 如何处理节点故障？试试这个：

```bash
# 1. 启动集群
./scripts/start_cluster.sh

# 2. 使用客户端写入一些数据
./build/bin/kv_client_cli
raft-kv> put test1 value1
raft-kv> put test2 value2
raft-kv> quit

# 3. 停止一个节点（模拟故障）
kill $(cat logs/node1.pid)

# 4. 继续写入数据（应该仍然成功）
./build/bin/kv_client_cli
raft-kv> put test3 value3
raft-kv> get test1
value1
raft-kv> quit

# 5. 重启故障节点
./build/bin/raft_node_server 1 config/cluster.conf > logs/node1.log 2>&1 &

# 6. 验证数据一致性
./build/bin/kv_client_cli
raft-kv> get test3
value3
raft-kv> quit
```

恭喜！你已经成功运行了一个 Raft 分布式 KV 存储集群！
