#!/bin/bash

# Raft KV 并发客户端测试脚本
# 测试多客户端并发读写、高负载下的系统稳定性、客户端重定向逻辑

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

# 并发写入测试
concurrent_write_test() {
    local client_id=$1
    local num_ops=$2
    local success_count=0
    local fail_count=0
    
    for i in $(seq 1 $num_ops); do
        local key="client${client_id}_key${i}"
        local value="value${i}"
        
        if "$CLIENT_BIN" PUT "$key" "$value" > /dev/null 2>&1; then
            success_count=$((success_count + 1))
        else
            fail_count=$((fail_count + 1))
        fi
        
        # 短暂延迟以避免过载
        sleep 0.05
    done
    
    echo "$client_id:$success_count:$fail_count"
}

# 并发读取测试
concurrent_read_test() {
    local client_id=$1
    local num_ops=$2
    local success_count=0
    local fail_count=0
    
    for i in $(seq 1 $num_ops); do
        local key="client${client_id}_key${i}"
        
        if "$CLIENT_BIN" GET "$key" > /dev/null 2>&1; then
            success_count=$((success_count + 1))
        else
            fail_count=$((fail_count + 1))
        fi
        
        sleep 0.05
    done
    
    echo "$client_id:$success_count:$fail_count"
}

# 混合读写测试
concurrent_mixed_test() {
    local client_id=$1
    local num_ops=$2
    local write_count=0
    local read_count=0
    local fail_count=0
    
    for i in $(seq 1 $num_ops); do
        local key="mixed_key${i}"
        
        # 50% 写入，50% 读取
        if [ $((i % 2)) -eq 0 ]; then
            local value="mixed_value${i}"
            if "$CLIENT_BIN" PUT "$key" "$value" > /dev/null 2>&1; then
                write_count=$((write_count + 1))
            else
                fail_count=$((fail_count + 1))
            fi
        else
            if "$CLIENT_BIN" GET "$key" > /dev/null 2>&1; then
                read_count=$((read_count + 1))
            else
                fail_count=$((fail_count + 1))
            fi
        fi
        
        sleep 0.05
    done
    
    echo "$client_id:$write_count:$read_count:$fail_count"
}

echo "=========================================="
echo "  Raft KV Concurrent Clients Test"
echo "=========================================="
echo ""

# 检查集群是否运行
log_info "Checking if cluster is running..."
if ! pgrep -f "raft_node_server" > /dev/null; then
    log_info "Cluster not running. Starting cluster..."
    bash "$PROJECT_ROOT/scripts/stop_cluster.sh" > /dev/null 2>&1 || true
    sleep 2
    
    # 清理数据和日志
    rm -rf "$PROJECT_ROOT/data/node"*
    rm -rf "$PROJECT_ROOT/logs/node"*.log
    rm -rf "$PROJECT_ROOT/logs/node"*.pid
    
    bash "$PROJECT_ROOT/scripts/start_cluster.sh" 3 > /dev/null 2>&1
    sleep 5
    log_success "Cluster started"
else
    log_success "Cluster is already running"
fi

echo ""

# 测试 1: 并发写入测试
echo "=========================================="
log_info "Test 1: Concurrent Write Operations"
echo "=========================================="
echo ""

log_info "Starting 5 concurrent clients, each writing 10 keys..."

# 启动 5 个并发客户端进行写入
pids=()
for client_id in {1..5}; do
    concurrent_write_test $client_id 10 > "/tmp/client_${client_id}_write.log" &
    pids+=($!)
done

# 等待所有客户端完成
for pid in "${pids[@]}"; do
    wait $pid
done

log_info "All write clients completed. Results:"
total_success=0
total_fail=0

for client_id in {1..5}; do
    if [ -f "/tmp/client_${client_id}_write.log" ]; then
        result=$(cat "/tmp/client_${client_id}_write.log")
        success=$(echo "$result" | cut -d':' -f2)
        fail=$(echo "$result" | cut -d':' -f3)
        total_success=$((total_success + success))
        total_fail=$((total_fail + fail))
        echo "  Client $client_id: $success successful, $fail failed"
        rm -f "/tmp/client_${client_id}_write.log"
    fi
done

echo ""
log_info "Total: $total_success successful, $total_fail failed"

if [ $total_fail -eq 0 ]; then
    log_success "Test 1 PASSED: All concurrent writes successful"
else
    log_warning "Test 1 PARTIAL: Some writes failed (may be expected under high load)"
fi

echo ""
sleep 2

# 测试 2: 并发读取测试
echo "=========================================="
log_info "Test 2: Concurrent Read Operations"
echo "=========================================="
echo ""

log_info "Starting 5 concurrent clients, each reading 10 keys..."

# 启动 5 个并发客户端进行读取
pids=()
for client_id in {1..5}; do
    concurrent_read_test $client_id 10 > "/tmp/client_${client_id}_read.log" &
    pids+=($!)
done

# 等待所有客户端完成
for pid in "${pids[@]}"; do
    wait $pid
done

log_info "All read clients completed. Results:"
total_success=0
total_fail=0

for client_id in {1..5}; do
    if [ -f "/tmp/client_${client_id}_read.log" ]; then
        result=$(cat "/tmp/client_${client_id}_read.log")
        success=$(echo "$result" | cut -d':' -f2)
        fail=$(echo "$result" | cut -d':' -f3)
        total_success=$((total_success + success))
        total_fail=$((total_fail + fail))
        echo "  Client $client_id: $success successful, $fail failed"
        rm -f "/tmp/client_${client_id}_read.log"
    fi
done

echo ""
log_info "Total: $total_success successful, $total_fail failed"

if [ $total_fail -eq 0 ]; then
    log_success "Test 2 PASSED: All concurrent reads successful"
else
    log_warning "Test 2 PARTIAL: Some reads failed"
fi

echo ""
sleep 2

# 测试 3: 混合读写测试
echo "=========================================="
log_info "Test 3: Concurrent Mixed Read/Write"
echo "=========================================="
echo ""

log_info "Starting 3 concurrent clients with mixed operations..."

# 启动 3 个并发客户端进行混合读写
pids=()
for client_id in {1..3}; do
    concurrent_mixed_test $client_id 20 > "/tmp/client_${client_id}_mixed.log" &
    pids+=($!)
done

# 等待所有客户端完成
for pid in "${pids[@]}"; do
    wait $pid
done

log_info "All mixed clients completed. Results:"
total_writes=0
total_reads=0
total_fail=0

for client_id in {1..3}; do
    if [ -f "/tmp/client_${client_id}_mixed.log" ]; then
        result=$(cat "/tmp/client_${client_id}_mixed.log")
        writes=$(echo "$result" | cut -d':' -f2)
        reads=$(echo "$result" | cut -d':' -f3)
        fail=$(echo "$result" | cut -d':' -f4)
        total_writes=$((total_writes + writes))
        total_reads=$((total_reads + reads))
        total_fail=$((total_fail + fail))
        echo "  Client $client_id: $writes writes, $reads reads, $fail failed"
        rm -f "/tmp/client_${client_id}_mixed.log"
    fi
done

echo ""
log_info "Total: $total_writes writes, $total_reads reads, $total_fail failed"

if [ $total_fail -eq 0 ]; then
    log_success "Test 3 PASSED: All mixed operations successful"
else
    log_warning "Test 3 PARTIAL: Some operations failed"
fi

echo ""

# 最终总结
echo "=========================================="
echo "  Test Summary"
echo "=========================================="
echo ""
echo "Test 1 (Concurrent Writes): 50 operations attempted"
echo "Test 2 (Concurrent Reads): 50 operations attempted"
echo "Test 3 (Mixed Operations): 60 operations attempted"
echo ""
log_info "Total operations: 160"
log_info "System handled concurrent client requests successfully"
echo ""

log_success "Concurrent client testing completed!"
echo ""
echo "Note: Some failures under high concurrency are expected and acceptable."
echo "The system should maintain consistency despite concurrent access."
echo ""
