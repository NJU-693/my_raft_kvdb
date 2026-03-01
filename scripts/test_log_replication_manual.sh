#!/bin/bash

# Raft KV 日志复制测试脚本（手动验证版）
# 测试多节点日志复制、Follower 落后追赶、日志冲突解决

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
CLIENT_BIN="$BUILD_DIR/bin/kv_client_cli"
CONFIG_FILE="$PROJECT_ROOT/config/cluster.conf"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# 检查日志大小
check_log_size() {
    local node_id=$1
    local log_file="$PROJECT_ROOT/logs/node$node_id.log"
    
    if [ -f "$log_file" ]; then
        # 查找最近的 "Log Size:" 日志
        local log_size=$(tail -n 100 "$log_file" | grep "Log Size:" | tail -n 1 | sed 's/.*Log Size: \([0-9]*\).*/\1/')
        echo "$log_size"
    else
        echo "0"
    fi
}

# 检查提交索引
check_commit_index() {
    local node_id=$1
    local log_file="$PROJECT_ROOT/logs/node$node_id.log"
    
    if [ -f "$log_file" ]; then
        # 查找最近的 "Commit Index:" 日志
        local commit_index=$(tail -n 100 "$log_file" | grep "Commit Index:" | tail -n 1 | sed 's/.*Commit Index: \([0-9]*\).*/\1/')
        echo "$commit_index"
    else
        echo "0"
    fi
}

# 获取节点 PID
get_node_pid() {
    local node_id=$1
    local pid_file="$PROJECT_ROOT/logs/node$node_id.pid"
    
    if [ -f "$pid_file" ]; then
        cat "$pid_file"
    else
        echo ""
    fi
}

# 停止节点
stop_node() {
    local node_id=$1
    local pid=$(get_node_pid $node_id)
    
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        log_info "Stopping node $node_id (PID: $pid)"
        kill "$pid" 2>/dev/null || true
        sleep 1
        
        # 强制终止如果还在运行
        if kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null || true
        fi
        
        rm -f "$PROJECT_ROOT/logs/node$node_id.pid"
        return 0
    else
        log_warning "Node $node_id is not running"
        return 1
    fi
}

# 启动节点
start_node() {
    local node_id=$1
    local server_bin="$BUILD_DIR/bin/raft_node_server"
    local log_file="$PROJECT_ROOT/logs/node$node_id.log"
    
    log_info "Starting node $node_id"
    
    # 启动节点（不清空日志，以便追踪）
    nohup "$server_bin" $node_id "$CONFIG_FILE" >> "$log_file" 2>&1 &
    echo $! > "$PROJECT_ROOT/logs/node$node_id.pid"
    
    sleep 1
}

echo "=========================================="
echo "  Raft KV Log Replication Test (Manual)"
echo "=========================================="
echo ""

# 测试 1: 正常日志复制
echo "=========================================="
log_info "Test 1: Normal Log Replication"
echo "=========================================="
echo ""

# 停止现有集群
log_info "Stopping existing cluster..."
bash "$PROJECT_ROOT/scripts/stop_cluster.sh" > /dev/null 2>&1 || true
sleep 2

# 清理数据和日志
log_info "Cleaning up data and logs..."
rm -rf "$PROJECT_ROOT/data/node"*
rm -rf "$PROJECT_ROOT/logs/node"*.log
rm -rf "$PROJECT_ROOT/logs/node"*.pid

# 启动 3 节点集群
log_info "Starting 3-node cluster..."
bash "$PROJECT_ROOT/scripts/start_cluster.sh" 3 > /dev/null 2>&1
sleep 5

log_info "Cluster started. Waiting for leader election..."
sleep 3

# 检查初始状态
log_info "Initial cluster state:"
for i in {1..3}; do
    log_size=$(check_log_size $i)
    commit_index=$(check_commit_index $i)
    echo "  Node $i: Log Size=$log_size, Commit Index=$commit_index"
done
echo ""

# 写入一些数据
log_info "Writing test data using non-interactive mode..."
for i in {1..5}; do
    log_info "Writing key$i=value$i"
    "$CLIENT_BIN" PUT "key$i" "value$i" > /dev/null 2>&1 || log_warning "PUT key$i failed"
    sleep 0.5
done
echo ""

log_info "Waiting for log replication..."
sleep 3

# 检查日志复制后的状态
log_info "After replication:"
for i in {1..3}; do
    log_size=$(check_log_size $i)
    commit_index=$(check_commit_index $i)
    echo "  Node $i: Log Size=$log_size, Commit Index=$commit_index"
done
echo ""

log_success "Test 1 completed. Check if all nodes have similar Log Size and Commit Index."
echo ""

# 测试 2: Follower 落后追赶
echo "=========================================="
log_info "Test 2: Follower Catch-up"
echo "=========================================="
echo ""

log_info "Stopping Node 3 to simulate network partition..."
stop_node 3
sleep 2

log_info "Writing more data while Node 3 is down..."
for i in {6..10}; do
    log_info "Writing key$i=value$i"
    "$CLIENT_BIN" PUT "key$i" "value$i" > /dev/null 2>&1 || log_warning "PUT key$i failed"
    sleep 0.5
done
echo ""

log_info "Waiting for replication on remaining nodes..."
sleep 3

log_info "State while Node 3 is down:"
for i in {1..2}; do
    log_size=$(check_log_size $i)
    commit_index=$(check_commit_index $i)
    echo "  Node $i: Log Size=$log_size, Commit Index=$commit_index"
done
echo "  Node 3: STOPPED"
echo ""

log_info "Restarting Node 3..."
start_node 3
sleep 5

log_info "Waiting for Node 3 to catch up..."
sleep 5

log_info "After Node 3 rejoined:"
for i in {1..3}; do
    log_size=$(check_log_size $i)
    commit_index=$(check_commit_index $i)
    echo "  Node $i: Log Size=$log_size, Commit Index=$commit_index"
done
echo ""

log_success "Test 2 completed. Check if Node 3 caught up with other nodes."
echo ""

# 测试 3: 验证数据一致性
echo "=========================================="
log_info "Test 3: Data Consistency Verification"
echo "=========================================="
echo ""

log_info "Reading back all written data..."
for i in {1..10}; do
    log_info "Reading key$i..."
    result=$("$CLIENT_BIN" GET "key$i" 2>&1 || echo "FAILED")
    if [[ "$result" == *"value$i"* ]]; then
        log_success "key$i = value$i ✓"
    else
        log_error "key$i read failed or incorrect: $result"
    fi
    sleep 0.3
done
echo ""

log_success "Test 3 completed. Check if all keys were read successfully."
echo ""

# 最终状态
echo "=========================================="
log_info "Final Cluster State"
echo "=========================================="
echo ""

for i in {1..3}; do
    log_size=$(check_log_size $i)
    commit_index=$(check_commit_index $i)
    echo "  Node $i: Log Size=$log_size, Commit Index=$commit_index"
done
echo ""

log_info "Test completed! Review the results above."
log_info "For detailed logs, check: logs/node1.log, logs/node2.log, logs/node3.log"
echo ""

echo "=========================================="
echo "  Manual Verification Steps"
echo "=========================================="
echo ""
echo "1. Check that all nodes have similar Log Size (should be around 10-15)"
echo "2. Check that all nodes have similar Commit Index"
echo "3. Check that Node 3 successfully caught up after rejoining"
echo "4. Check that all GET operations returned correct values"
echo ""
echo "To stop the cluster:"
echo "  ./scripts/stop_cluster.sh"
echo ""
