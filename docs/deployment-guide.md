# Raft KV Store 部署指南

本文档介绍如何编译、部署和运行 Raft KV 分布式存储系统。

## 目录

- [系统要求](#系统要求)
- [编译项目](#编译项目)
- [配置集群](#配置集群)
- [启动集群](#启动集群)
- [使用客户端](#使用客户端)
- [管理集群](#管理集群)
- [故障排除](#故障排除)

## 系统要求

### 必需依赖

- C++17 编译器（GCC 7+ 或 Clang 5+）
- CMake 3.14+
- gRPC 和 Protocol Buffers
- pthread 库

### 可选依赖

- Google Test（用于运行测试）

### 安装依赖（Ubuntu/Debian）

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc
```

## 编译项目

### 1. 克隆或下载项目

```bash
cd /path/to/raft-kv-store
```

### 2. 创建构建目录

```bash
mkdir -p build
cd build
```

### 3. 配置 CMake

```bash
cmake ..
```

如果需要编译测试和示例：

```bash
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
```

### 4. 编译

```bash
make -j$(nproc)
```

编译成功后，可执行文件位于 `build/bin/` 目录：

- `raft_node_server` - Raft 节点服务器
- `kv_client_cli` - KV 客户端命令行工具

## 配置集群

### 配置文件位置

默认配置文件：`config/cluster.conf`

### 配置文件格式

```ini
# 集群配置
[cluster]
node_count = 3

# 节点 1 配置
[node_1]
id = 1
host = 127.0.0.1
port = 8001
data_dir = ./data/node1

# 节点 2 配置
[node_2]
id = 2
host = 127.0.0.1
port = 8002
data_dir = ./data/node2

# 节点 3 配置
[node_3]
id = 3
host = 127.0.0.1
port = 8003
data_dir = ./data/node3

# Raft 算法参数
[raft]
election_timeout_min = 150
election_timeout_max = 300
heartbeat_interval = 50
rpc_timeout = 1000
```

### 配置说明

- `node_count`: 集群节点总数
- `id`: 节点唯一标识符（从 1 开始）
- `host`: 节点监听地址
- `port`: 节点监听端口
- `data_dir`: 持久化数据存储目录
- `election_timeout_min/max`: 选举超时时间范围（毫秒）
- `heartbeat_interval`: 心跳间隔（毫秒）
- `rpc_timeout`: RPC 超时时间（毫秒）

## 启动集群

### 方法 1：使用启动脚本（推荐）

```bash
# 从项目根目录执行
chmod +x scripts/*.sh

# 启动 3 节点集群
./scripts/start_cluster.sh

# 或指定节点数量
./scripts/start_cluster.sh 5
```

### 方法 2：手动启动节点

```bash
# 创建数据目录
mkdir -p data/node1 data/node2 data/node3

# 启动节点 1
./build/bin/raft_node_server 1 config/cluster.conf &

# 启动节点 2
./build/bin/raft_node_server 2 config/cluster.conf &

# 启动节点 3
./build/bin/raft_node_server 3 config/cluster.conf &
```

### 验证集群状态

```bash
# 检查集群状态
./scripts/check_cluster.sh

# 查看节点日志
tail -f logs/node1.log
tail -f logs/node2.log
tail -f logs/node3.log
```

## 使用客户端

### 启动客户端

```bash
./build/bin/kv_client_cli config/cluster.conf
```

### 客户端命令

```
raft-kv> put key1 value1
OK

raft-kv> get key1
value1

raft-kv> put key2 hello world
OK

raft-kv> get key2
hello world

raft-kv> delete key1
OK

raft-kv> get key1
Error: Key not found

raft-kv> help
Usage: kv_client_cli [config_file]
  config_file: Path to cluster configuration file

Commands:
  put <key> <value>  - Store a key-value pair
  get <key>          - Retrieve value for a key
  delete <key>       - Delete a key-value pair
  help               - Show this help message
  quit               - Exit the client

raft-kv> quit
```

## 管理集群

### 停止集群

```bash
# 使用脚本停止所有节点
./scripts/stop_cluster.sh

# 或手动停止
pkill -f raft_node_server
```

### 重启集群

```bash
./scripts/restart_cluster.sh
```

### 查看集群状态

```bash
./scripts/check_cluster.sh
```

### 查看日志

```bash
# 实时查看所有节点日志
tail -f logs/node*.log

# 查看特定节点日志
tail -f logs/node1.log
```

### 清理数据

```bash
# 清理持久化数据（谨慎操作！）
rm -rf data/node*

# 清理日志
rm -rf logs/*.log logs/*.pid
```

## 故障排除

### 问题 1：节点无法启动

**症状**：节点启动后立即退出

**可能原因**：
- 端口已被占用
- 配置文件路径错误
- 数据目录权限不足

**解决方法**：
```bash
# 检查端口占用
netstat -tuln | grep 800[1-3]

# 检查配置文件
cat config/cluster.conf

# 检查数据目录权限
ls -la data/
```

### 问题 2：客户端无法连接

**症状**：客户端报告连接失败

**可能原因**：
- 集群未启动
- 网络配置错误
- 防火墙阻止连接

**解决方法**：
```bash
# 检查节点是否运行
./scripts/check_cluster.sh

# 测试网络连接
telnet 127.0.0.1 8001
```

### 问题 3：选举失败

**症状**：日志显示持续选举但无 Leader

**可能原因**：
- 节点数量不足（需要多数节点在线）
- 网络分区
- 时钟不同步

**解决方法**：
```bash
# 确保至少 (N/2 + 1) 个节点在线
# 对于 3 节点集群，至少需要 2 个节点

# 检查节点日志
grep "Election" logs/node*.log
```

### 问题 4：日志复制失败

**症状**：写入操作超时或失败

**可能原因**：
- Leader 不可用
- 网络延迟过高
- 节点负载过高

**解决方法**：
```bash
# 检查 Leader 状态
grep "LEADER" logs/node*.log

# 检查日志复制
grep "AppendEntries" logs/node*.log
```

### 问题 5：数据不一致

**症状**：不同节点返回不同的值

**可能原因**：
- 读取了未提交的数据
- 节点状态未同步

**解决方法**：
```bash
# 重启集群
./scripts/restart_cluster.sh

# 检查提交索引
grep "Commit Index" logs/node*.log
```

## 性能调优

### 调整 Raft 参数

编辑 `config/cluster.conf`：

```ini
[raft]
# 降低选举超时以加快故障恢复
election_timeout_min = 100
election_timeout_max = 200

# 增加心跳频率以减少选举
heartbeat_interval = 30

# 增加 RPC 超时以适应高延迟网络
rpc_timeout = 2000
```

### 监控指标

关键指标：
- 选举频率（应该很低）
- 日志复制延迟
- 提交延迟
- 吞吐量（ops/sec）

## 生产环境部署建议

1. **节点数量**：使用奇数个节点（3、5、7）
2. **网络**：确保节点间低延迟、高带宽连接
3. **存储**：使用 SSD 以提高持久化性能
4. **监控**：部署监控系统跟踪关键指标
5. **备份**：定期备份持久化数据
6. **安全**：使用 TLS 加密节点间通信（需要额外配置）

## 下一步

- 阅读 [架构文档](architecture.md) 了解系统设计
- 阅读 [Raft 算法说明](raft-algorithm.md) 了解共识算法
- 查看 [示例程序](../examples/README.md) 学习核心机制
- 运行 [测试套件](../test/) 验证系统功能

## 获取帮助

如果遇到问题：
1. 查看日志文件 `logs/node*.log`
2. 检查配置文件 `config/cluster.conf`
3. 参考本文档的故障排除部分
4. 查看项目 README 和其他文档
