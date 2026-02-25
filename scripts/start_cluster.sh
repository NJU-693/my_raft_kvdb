#!/bin/bash

# Raft KV 集群启动脚本
# 用法: ./start_cluster.sh [node_count]

set -e

# 默认启动 3 个节点
NODE_COUNT=${1:-3}
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
CONFIG_FILE="$PROJECT_ROOT/config/cluster.conf"
SERVER_BIN="$BUILD_DIR/bin/raft_node_server"

echo "=========================================="
echo "  Starting Raft KV Cluster"
echo "=========================================="
echo "Project Root: $PROJECT_ROOT"
echo "Node Count: $NODE_COUNT"
echo "Config File: $CONFIG_FILE"
echo ""

# 检查可执行文件是否存在
if [ ! -f "$SERVER_BIN" ]; then
    echo "Error: Server binary not found at $SERVER_BIN"
    echo "Please build the project first:"
    echo "  cd build && cmake .. && make"
    exit 1
fi

# 检查配置文件是否存在
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file not found at $CONFIG_FILE"
    exit 1
fi

# 创建日志目录
mkdir -p "$PROJECT_ROOT/logs"

# 创建数据目录
for i in $(seq 1 $NODE_COUNT); do
    mkdir -p "$PROJECT_ROOT/data/node$i"
done

# 启动节点
echo "Starting nodes..."
for i in $(seq 1 $NODE_COUNT); do
    LOG_FILE="$PROJECT_ROOT/logs/node$i.log"
    echo "  Starting node $i (log: $LOG_FILE)"
    
    # 在后台启动节点，输出重定向到日志文件
    nohup "$SERVER_BIN" $i "$CONFIG_FILE" > "$LOG_FILE" 2>&1 &
    
    # 保存 PID
    echo $! > "$PROJECT_ROOT/logs/node$i.pid"
    
    # 短暂延迟，避免端口冲突
    sleep 0.5
done

echo ""
echo "=========================================="
echo "  Cluster started successfully!"
echo "=========================================="
echo ""
echo "Node status:"
for i in $(seq 1 $NODE_COUNT); do
    PID=$(cat "$PROJECT_ROOT/logs/node$i.pid" 2>/dev/null || echo "N/A")
    echo "  Node $i: PID $PID"
done

echo ""
echo "To view logs:"
for i in $(seq 1 $NODE_COUNT); do
    echo "  tail -f logs/node$i.log"
done

echo ""
echo "To stop the cluster:"
echo "  ./scripts/stop_cluster.sh"
echo ""
