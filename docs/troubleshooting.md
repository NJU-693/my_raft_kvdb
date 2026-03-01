# 故障排查指南

本文档帮助你诊断和解决 Raft KV Store 的常见问题。

## 目录

- [编译问题](#编译问题)
- [启动问题](#启动问题)
- [运行时问题](#运行时问题)
- [性能问题](#性能问题)
- [数据一致性问题](#数据一致性问题)
- [网络问题](#网络问题)

## 编译问题

### 问题：找不到 gRPC 库

**错误信息：**
```
CMake Error: Could not find gRPC
```

**解决方案：**

Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
```

macOS:
```bash
brew install grpc protobuf
```

从源码编译:
```bash
git clone --recurse-submodules -b v1.50.0 https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build && cd cmake/build
cmake ../..
make -j$(nproc)
sudo make install
```

### 问题：C++17 特性不支持

**错误信息：**
```
error: 'structured bindings' are a C++17 extension
```

**解决方案：**

检查编译器版本：
```bash
g++ --version  # 需要 GCC 7+
clang++ --version  # 需要 Clang 6+
```

升级编译器或在 CMakeLists.txt 中指定：
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### 问题：链接错误

**错误信息：**
```
undefined reference to `pthread_create'
```

**解决方案：**

确保链接了 pthread 库：
```cmake
target_link_libraries(your_target pthread)
```

## 启动问题

### 问题：端口已被占用

**错误信息：**
```
Failed to bind to port 8001: Address already in use
```

**诊断：**
```bash
# 检查端口占用
netstat -tuln | grep 8001
# 或
lsof -i :8001
```

**解决方案：**

方案 1：停止占用端口的进程
```bash
# 找到进程 ID
lsof -ti :8001
# 停止进程
kill $(lsof -ti :8001)
```

方案 2：修改配置文件
```bash
# 编辑 config/cluster.conf
# 修改端口号为未占用的端口
```

### 问题：节点启动后立即退出

**诊断：**
```bash
# 查看节点日志
cat logs/node1.log

# 检查进程状态
ps aux | grep raft_node_server
```

**常见原因：**

1. 配置文件错误
```bash
# 检查配置文件格式
cat config/cluster.conf
```

2. 数据目录权限问题
```bash
# 检查权限
ls -la data/
# 修复权限
chmod 755 data/
```

3. 依赖库缺失
```bash
# 检查依赖
ldd build/bin/raft_node_server
```

### 问题：无法创建数据目录

**错误信息：**
```
Failed to create data directory: Permission denied
```

**解决方案：**
```bash
# 创建目录并设置权限
mkdir -p data/node1 data/node2 data/node3
chmod 755 data/node*
```

## 运行时问题

### 问题：客户端无法连接

**错误信息：**
```
Error: Max retries exceeded
```

**诊断步骤：**

1. 检查集群状态
```bash
./scripts/check_cluster.sh
```

2. 检查网络连接
```bash
# 测试连接
telnet localhost 8001
# 或
nc -zv localhost 8001
```

3. 检查防火墙
```bash
# Ubuntu/Debian
sudo ufw status
# CentOS/RHEL
sudo firewall-cmd --list-all
```

**解决方案：**

1. 确保集群运行
```bash
./scripts/start_cluster.sh
```

2. 检查配置文件中的地址
```bash
cat config/cluster.conf
```

3. 关闭防火墙或添加规则
```bash
# Ubuntu/Debian
sudo ufw allow 8001:8003/tcp
```

### 问题：GET 操作超时

**症状：**
- PUT/DELETE 正常
- GET 操作超时或失败

**诊断：**
```bash
# 查看 Leader 日志
tail -f logs/node*.log | grep "submitCommand\|applyCommitted"
```

**解决方案：**

1. 检查 Leader 是否正常应用日志
2. 重启集群
```bash
./scripts/restart_cluster.sh
```

3. 如果问题持续，查看详细日志

### 问题：节点无法选举出 Leader

**症状：**
- 所有节点都是 FOLLOWER 或 CANDIDATE
- 日志中频繁出现选举超时

**诊断：**
```bash
# 查看所有节点状态
grep "State:" logs/node*.log | tail -20
```

**可能原因：**

1. 网络分区
```bash
# 检查节点间连接
for i in {1..3}; do
    for j in {1..3}; do
        if [ $i -ne $j ]; then
            echo "Testing node$i -> node$j"
            nc -zv localhost 800$j
        fi
    done
done
```

2. 时钟不同步
```bash
# 检查系统时间
date
# 同步时间
sudo ntpdate pool.ntp.org
```

3. 配置错误
```bash
# 检查节点 ID 是否唯一
grep "Node ID:" logs/node*.log
```

**解决方案：**

1. 重启集群
```bash
./scripts/restart_cluster.sh
```

2. 检查并修复网络问题
3. 确保配置正确

### 问题：节点崩溃

**诊断：**
```bash
# 查看崩溃日志
tail -100 logs/node1.log

# 检查系统日志
dmesg | tail -50

# 检查核心转储
ls -la core*
```

**常见原因：**

1. 内存不足
```bash
free -h
```

2. 段错误（Segmentation Fault）
```bash
# 使用 gdb 调试
gdb build/bin/raft_node_server core
(gdb) bt
```

3. 断言失败
```bash
# 查看日志中的 assertion
grep "Assertion" logs/node*.log
```

## 性能问题

### 问题：QPS 过低

**症状：**
- 操作延迟高（> 200ms）
- QPS < 5 ops/sec

**诊断：**
```bash
# 运行性能测试
./scripts/test_performance.sh
```

**可能原因和解决方案：**

1. 网络延迟高
```bash
# 测试网络延迟
ping localhost
```

2. 磁盘 I/O 慢
```bash
# 测试磁盘性能
dd if=/dev/zero of=test.dat bs=1M count=100
```

3. CPU 负载高
```bash
# 查看 CPU 使用率
top
htop
```

4. 日志过多
```bash
# 清理旧日志
rm -f logs/*.log
```

### 问题：内存使用过高

**诊断：**
```bash
# 查看内存使用
ps aux | grep raft_node_server
top -p $(pgrep raft_node_server)
```

**解决方案：**

1. 实现日志压缩（Snapshot）
2. 限制日志大小
3. 定期清理旧数据

### 问题：并发性能差

**症状：**
- 单客户端性能正常
- 多客户端性能不提升

**诊断：**
```bash
# 测试并发性能
./scripts/test_concurrent_clients.sh
```

**解决方案：**

1. 增加 RPC 线程池大小（在 `raft_node_server.cpp` 中）
2. 优化锁粒度
3. 使用连接池

## 数据一致性问题

### 问题：不同节点数据不一致

**诊断：**
```bash
# 检查所有节点的日志大小
grep "Log Size:" logs/node*.log | tail -10

# 检查 commitIndex
grep "Commit Index:" logs/node*.log | tail -10
```

**解决方案：**

1. 等待日志复制完成
```bash
# 等待几秒后再次检查
sleep 5
grep "Log Size:" logs/node*.log | tail -3
```

2. 检查是否有网络分区
3. 重启落后的节点

### 问题：数据丢失

**症状：**
- 写入的数据无法读取
- 节点重启后数据消失

**诊断：**
```bash
# 检查持久化文件
ls -la data/node*/raft_state.dat

# 检查文件大小
du -h data/node*/raft_state.dat
```

**可能原因：**

1. 持久化未启用
2. 磁盘空间不足
```bash
df -h
```

3. 文件权限问题
```bash
ls -la data/node*
```

**解决方案：**

1. 确保持久化功能正常
2. 清理磁盘空间
3. 修复文件权限

### 问题：脑裂（Split Brain）

**症状：**
- 出现多个 Leader
- 数据冲突

**诊断：**
```bash
# 检查 Leader 数量
grep "Became LEADER" logs/node*.log | tail -10
```

**解决方案：**

1. 立即停止集群
```bash
./scripts/stop_cluster.sh
```

2. 检查配置
```bash
cat config/cluster.conf
```

3. 清理数据并重启
```bash
rm -rf data/node*/*
./scripts/start_cluster.sh
```

## 网络问题

### 问题：网络分区

**症状：**
- 部分节点无法通信
- 频繁的 Leader 切换

**诊断：**
```bash
# 测试节点间连接
for i in {1..3}; do
    for j in {1..3}; do
        if [ $i -ne $j ]; then
            timeout 2 bash -c "echo > /dev/tcp/localhost/800$j" && \
                echo "node$i -> node$j: OK" || \
                echo "node$i -> node$j: FAILED"
        fi
    done
done
```

**解决方案：**

1. 检查防火墙规则
2. 检查网络配置
3. 重启网络服务

### 问题：RPC 超时

**错误信息：**
```
RPC timeout: deadline exceeded
```

**解决方案：**

1. 增加超时时间（在代码中修改）
2. 优化网络配置
3. 减少负载

## 调试技巧

### 启用详细日志

在 `raft_node_server.cpp` 中添加更多日志：

```cpp
std::cout << "[DEBUG] Current state: " << state << std::endl;
std::cout << "[DEBUG] Term: " << currentTerm << std::endl;
```

### 使用 GDB 调试

```bash
# 编译 Debug 版本
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# 启动 GDB
gdb --args build/bin/raft_node_server 1 config/cluster.conf

# 设置断点
(gdb) break RaftNode::submitCommand
(gdb) run
```

### 使用 Valgrind 检测内存泄漏

```bash
valgrind --leak-check=full --show-leak-kinds=all \
    build/bin/raft_node_server 1 config/cluster.conf
```

### 查看系统调用

```bash
strace -f build/bin/raft_node_server 1 config/cluster.conf
```

## 获取帮助

如果以上方法都无法解决问题：

1. 收集以下信息：
   - 错误信息和日志
   - 系统环境（OS、编译器版本）
   - 配置文件
   - 重现步骤

2. 查看文档：
   - [架构文档](architecture.md)
   - [部署指南](deployment-guide.md)
   - [API 参考](api-reference.md)

3. 提交 Issue（如果是开源项目）

## 预防措施

### 定期备份

```bash
# 备份数据目录
tar -czf backup-$(date +%Y%m%d).tar.gz data/
```

### 监控集群状态

```bash
# 定期检查集群
watch -n 5 './scripts/check_cluster.sh'
```

### 日志轮转

```bash
# 配置 logrotate
cat > /etc/logrotate.d/raft-kv << EOF
/path/to/logs/*.log {
    daily
    rotate 7
    compress
    missingok
    notifempty
}
EOF
```

### 资源监控

```bash
# 监控资源使用
while true; do
    echo "=== $(date) ==="
    ps aux | grep raft_node_server | grep -v grep
    free -h
    df -h
    sleep 60
done > resource_monitor.log
```

## 相关文档

- [部署指南](deployment-guide.md)
- [性能测试](performance-test-results.md)
- [架构设计](architecture.md)
