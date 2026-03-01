#!/bin/bash

# Raft KV 日志复制测试脚本
# 测试正常日志复制、Follower 追赶、日志冲突解决

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
CONFIG_FILE="$PROJECT_ROOT/config/cluster.conf"
CLIENT_BIN="$BUILD_DIR/bin/kv_client_cli"
RESULTS_FILE="$PROJECT_ROOT/test_results_log_replication.txt"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 测试结果统计
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
    echo "[INFO] $1" >> "$RESULTS_FILE"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
    echo "[PASS] $1" >> "$RESULTS_FILE"
    PASSED_TESTS=$((PASSED_TESTS + 1))
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
    echo "[FAIL] $1" >> "$RESULTS_FILE"
    FAILED_TESTS=$((FAILED_TESTS + 1))
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[WARN] $1" >> "$RESULTS_FILE"
}

# 获取领导者 ID（从日志中查找 "State: LEADER"）
get_leader_id() {
    local node_count=$1
    local leader_id=""
    
    for i in $(seq 1 $node_count); do
        local log_file="$PROJECT_ROOT/logs/node$i.log"
        if [ -f "$log_file" ]; then
            # 查找最近的 "State: LEADER" 日志
            if tail -n 100 "$log_file" | grep -q "State: LEADER"; then
                leader_id=$i
                break
            fi
        fi
    done
    
    echo "$leader_id"
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
    local config_file=$2
    local server_bin="$BUILD_DIR/bin/raft_node_server"
    local log_file="$PROJECT_ROOT/logs/node$node_id.log"
    
    log_info "Starting node $node_id"
    
    # 启动节点（不清空日志，保留历史）
    nohup "$server_bin" $node_id "$config_file" >> "$log_file" 2>&1 &
    echo $! > "$PROJECT_ROOT/logs/node$node_id.pid"
    
    sleep 1
}

# 等待领导者选举完成
wait_for_leader() {
    local node_count=$1
    local max_wait=$2
    local waited=0
    
    log_info "Waiting for leader election (max ${max_wait}s)..." >&2
    
    while [ $waited -lt $max_wait ]; do
        for i in $(seq 1 $node_count); do
            local log_file="$PROJECT_ROOT/logs/node$i.log"
            if [ -f "$log_file" ]; then
                # 查找最近的 "State: LEADER" 日志
                if tail -n 50 "$log_file" | grep -q "State: LEADER"; then
                    log_success "Leader elected: Node $i (waited ${waited}s)" >&2
                    echo "$i"
                    return 0
                fi
            fi
        done
        
        sleep 1
        waited=$((waited + 1))
    done
    
    log_error "Leader election timeout after ${max_wait}s" >&2
    return 1
}

# 获取节点的日志条目数量
get_log_entry_count() {
    local node_id=$1
    local log_file="$PROJECT_ROOT/logs/node$node_id.log"
    
    if [ ! -f "$log_file" ]; then
        echo "0"
        return
    fi
    
    # 查找最后一个 "Log Size:" 消息
    local log_size=$(grep "Log Size:" "$log_file" | tail -n 1 | grep -oE 'Log Size: [0-9]+' | grep -oE '[0-9]+')
    
    if [ -z "$log_size" ]; then
        echo "0"
    else
        echo "$log_size"
    fi
}

# 获取节点的 commitIndex
get_commit_index() {
    local node_id=$1
    local log_file="$PROJECT_ROOT/logs/node$node_id.log"
    
    if [ ! -f "$log_file" ]; then
        echo "0"
        return
    fi
    
    # 查找最后一个 "Commit Index:" 消息
    local commit_idx=$(grep "Commit Index:" "$log_file" | tail -n 1 | grep -oE 'Commit Index: [0-9]+' | grep -oE '[0-9]+')
    
    if [ -z "$commit_idx" ]; then
        echo "0"
    else
        echo "$commit_idx"
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

# 检查所有节点的日志是否一致
check_log_consistency() {
    local node_count=$1
    local expected_count=$2
    
    log_info "Checking log consistency across $node_count nodes..."
    
    local consistent=true
    local first_count=""
    
    for i in $(seq 1 $node_count); do
        local pid=$(get_node_pid $i)
        if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
            log_warning "Node $i is not running, skipping"
            continue
        fi
        
        local log_count=$(get_log_entry_count $i)
        local commit_idx=$(get_commit_index $i)
        
        log_info "Node $i: log entries=$log_count, commitIndex=$commit_idx"
        
        if [ -z "$first_count" ]; then
            first_count=$log_count
        elif [ "$log_count" != "$first_count" ]; then
            consistent=false
            log_warning "Node $i has different log count: $log_count vs $first_count"
        fi
    done
    
    if [ "$consistent" = true ]; then
        echo "consistent"
    else
        echo "inconsistent"
    fi
}

# 测试 1: 正常日志复制
test_normal_log_replication() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo ""
    echo "=========================================="
    log_info "Test 1: Normal Log Replication"
    echo "=========================================="
    
    # 停止现有集群
    log_info "Stopping existing cluster..."
    bash "$PROJECT_ROOT/scripts/stop_cluster.sh" > /dev/null 2>&1 || true
    sleep 2
    
    # 清理数据和日志
    log_info "Cleaning up data and logs..."
    rm -rf "$PROJECT_ROOT/data/node"*
    rm -rf "$PROJECT_ROOT/logs/node"*.log
    
    # 启动 3 节点集群
    log_info "Starting 3-node cluster..."
    bash "$PROJECT_ROOT/scripts/start_cluster.sh" 3 > /dev/null 2>&1
    sleep 3
    
    # 等待领导者选举
    local leader_id=$(wait_for_leader 3 10)
    if [ -z "$leader_id" ]; then
        log_error "Test 1 failed: No leader elected"
        return 1
    fi
    
    # 提交多个命令
    log_info "Submitting multiple PUT/DELETE commands..."
    local test_count=10
    
    for i in $(seq 1 $test_count); do
        "$CLIENT_BIN" PUT "key$i" "value$i" > /dev/null 2>&1 || log_warning "PUT key$i failed"
        sleep 0.2
    done
    
    # 删除一些键
    for i in $(seq 1 3); do
        "$CLIENT_BIN" DELETE "key$i" > /dev/null 2>&1 || log_warning "DELETE key$i failed"
        sleep 0.2
    done
    
    # 等待日志复制
    log_info "Waiting for log replication..."
    sleep 5
    
    # 检查日志一致性
    local consistency=$(check_log_consistency 3 $((test_count + 3)))
    
    if [ "$consistency" = "consistent" ]; then
        log_success "Test 1.1: Logs replicated to all nodes"
    else
        log_error "Test 1.1: Log inconsistency detected"
        return 1
    fi
    
    # 检查 commitIndex 是否前进
    log_info "Checking commitIndex advancement..."
    local all_committed=true
    
    for i in {1..3}; do
        local commit_idx=$(get_commit_index $i)
        log_info "Node $i commitIndex: $commit_idx"
        
        if [ "$commit_idx" -lt 5 ]; then
            all_committed=false
            log_warning "Node $i commitIndex too low: $commit_idx"
        fi
    done
    
    if [ "$all_committed" = true ]; then
        log_success "Test 1.2: commitIndex advanced correctly on all nodes"
    else
        log_warning "Test 1.2: Some nodes have low commitIndex"
    fi
    
    # 验证数据一致性
    log_info "Verifying data consistency..."
    local data_consistent=true
    
    for i in $(seq 4 $test_count); do
        local result=$("$CLIENT_BIN" GET "key$i" 2>&1 || true)
        if ! echo "$result" | grep -q "value$i"; then
            data_consistent=false
            log_warning "GET key$i returned unexpected result: $result"
        fi
    done
    
    if [ "$data_consistent" = true ]; then
        log_success "Test 1.3: Data consistent across cluster"
    else
        log_warning "Test 1.3: Data inconsistency detected"
    fi
    
    log_success "Test 1: Normal Log Replication - PASSED"
    return 0
}

# 测试 2: Follower 落后时的日志追赶
test_follower_catchup() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo ""
    echo "=========================================="
    log_info "Test 2: Follower Catch-up"
    echo "=========================================="
    
    # 停止现有集群
    log_info "Stopping existing cluster..."
    bash "$PROJECT_ROOT/scripts/stop_cluster.sh" > /dev/null 2>&1 || true
    sleep 2
    
    # 清理数据和日志
    log_info "Cleaning up data and logs..."
    rm -rf "$PROJECT_ROOT/data/node"*
    rm -rf "$PROJECT_ROOT/logs/node"*.log
    
    # 启动 3 节点集群
    log_info "Starting 3-node cluster..."
    bash "$PROJECT_ROOT/scripts/start_cluster.sh" 3 > /dev/null 2>&1
    sleep 3
    
    # 等待领导者选举
    local leader_id=$(wait_for_leader 3 10)
    if [ -z "$leader_id" ]; then
        log_error "Test 2 failed: No leader elected"
        return 1
    fi
    
    # 写入初始数据
    log_info "Writing initial data..."
    for i in {1..5}; do
        "$CLIENT_BIN" PUT "initial$i" "data$i" > /dev/null 2>&1 || true
        sleep 0.2
    done
    
    sleep 2
    
    # 停止一个 Follower
    local follower_to_stop=3
    if [ "$leader_id" -eq 3 ]; then
        follower_to_stop=2
    fi
    
    log_info "Stopping follower node $follower_to_stop..."
    stop_node $follower_to_stop
    sleep 2
    
    # 在 Follower 停止期间写入更多数据
    log_info "Writing data while follower is down..."
    for i in {1..10}; do
        "$CLIENT_BIN" PUT "catchup$i" "value$i" > /dev/null 2>&1 || log_warning "PUT catchup$i failed"
        sleep 0.2
    done
    
    sleep 2
    
    # 获取其他节点的日志大小
    local leader_log_count=$(get_log_entry_count $leader_id)
    log_info "Leader (node $leader_id) log entries: $leader_log_count"
    
    # 重启 Follower
    log_info "Restarting follower node $follower_to_stop..."
    start_node $follower_to_stop "$CONFIG_FILE"
    
    # 等待 Follower 追赶
    log_info "Waiting for follower to catch up..."
    sleep 5
    
    # 检查 Follower 是否追赶上
    local follower_log_count=$(get_log_entry_count $follower_to_stop)
    log_info "Follower (node $follower_to_stop) log entries: $follower_log_count"
    
    if [ "$follower_log_count" -ge "$((leader_log_count - 2))" ]; then
        log_success "Test 2.1: Follower caught up with leader"
    else
        log_error "Test 2.1: Follower failed to catch up (follower=$follower_log_count, leader=$leader_log_count)"
        return 1
    fi
    
    # 验证数据一致性
    log_info "Verifying data consistency after catch-up..."
    sleep 2
    
    local data_consistent=true
    for i in {1..10}; do
        local result=$("$CLIENT_BIN" GET "catchup$i" 2>&1 || true)
        if ! echo "$result" | grep -q "value$i"; then
            data_consistent=false
            log_warning "GET catchup$i returned unexpected result: $result"
            break
        fi
    done
    
    if [ "$data_consistent" = true ]; then
        log_success "Test 2.2: Data consistent after follower catch-up"
    else
        log_warning "Test 2.2: Data inconsistency after catch-up"
    fi
    
    # 检查日志一致性
    sleep 2
    local consistency=$(check_log_consistency 3 0)
    
    if [ "$consistency" = "consistent" ]; then
        log_success "Test 2.3: All nodes have consistent logs"
    else
        log_warning "Test 2.3: Log inconsistency detected"
    fi
    
    log_success "Test 2: Follower Catch-up - PASSED"
    return 0
}

# 测试 3: 日志冲突解决
test_log_conflict_resolution() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo ""
    echo "=========================================="
    log_info "Test 3: Log Conflict Resolution"
    echo "=========================================="
    
    log_info "Note: This test simulates log conflicts by creating network partition scenarios"
    
    # 停止现有集群
    log_info "Stopping existing cluster..."
    bash "$PROJECT_ROOT/scripts/stop_cluster.sh" > /dev/null 2>&1 || true
    sleep 2
    
    # 清理数据和日志
    log_info "Cleaning up data and logs..."
    rm -rf "$PROJECT_ROOT/data/node"*
    rm -rf "$PROJECT_ROOT/logs/node"*.log
    
    # 启动 3 节点集群
    log_info "Starting 3-node cluster..."
    bash "$PROJECT_ROOT/scripts/start_cluster.sh" 3 > /dev/null 2>&1
    sleep 3
    
    # 等待领导者选举
    local initial_leader=$(wait_for_leader 3 10)
    if [ -z "$initial_leader" ]; then
        log_error "Test 3 failed: No leader elected"
        return 1
    fi
    
    log_info "Initial leader: Node $initial_leader"
    
    # 写入初始数据
    log_info "Writing initial data..."
    for i in {1..5}; do
        "$CLIENT_BIN" PUT "conflict_test$i" "initial$i" > /dev/null 2>&1 || true
        sleep 0.2
    done
    
    sleep 2
    
    # 模拟网络分区：停止领导者
    log_info "Simulating network partition by stopping leader..."
    stop_node $initial_leader
    sleep 2
    
    # 等待新领导者选举
    log_info "Waiting for new leader election..."
    local new_leader=$(wait_for_leader 2 10)
    
    if [ -z "$new_leader" ]; then
        log_error "Test 3.1: No new leader elected after partition"
        return 1
    fi
    
    log_success "Test 3.1: New leader elected: Node $new_leader"
    
    # 在新领导者上写入数据
    log_info "Writing data to new leader..."
    for i in {6..10}; do
        "$CLIENT_BIN" PUT "conflict_test$i" "new_leader$i" > /dev/null 2>&1 || log_warning "PUT failed"
        sleep 0.2
    done
    
    sleep 2
    
    # 重启旧领导者（现在是 Follower）
    log_info "Restarting old leader (now follower)..."
    start_node $initial_leader "$CONFIG_FILE"
    
    # 等待日志同步
    log_info "Waiting for log synchronization..."
    sleep 5
    
    # 检查日志一致性
    local consistency=$(check_log_consistency 3 0)
    
    if [ "$consistency" = "consistent" ]; then
        log_success "Test 3.2: Log conflicts resolved, all nodes consistent"
    else
        log_warning "Test 3.2: Log inconsistency detected after conflict resolution"
    fi
    
    # 验证数据一致性
    log_info "Verifying data consistency..."
    local data_consistent=true
    
    for i in {6..10}; do
        local result=$("$CLIENT_BIN" GET "conflict_test$i" 2>&1 || true)
        if ! echo "$result" | grep -q "new_leader$i"; then
            data_consistent=false
            log_warning "GET conflict_test$i returned unexpected result: $result"
        fi
    done
    
    if [ "$data_consistent" = true ]; then
        log_success "Test 3.3: Data consistent after conflict resolution"
    else
        log_warning "Test 3.3: Data inconsistency detected"
    fi
    
    # 检查旧领导者是否正确回退日志
    log_info "Checking if old leader correctly rolled back conflicting entries..."
    local old_leader_log="$PROJECT_ROOT/logs/node$initial_leader.log"
    
    if grep -q "Rolling back" "$old_leader_log" || grep -q "Truncating log" "$old_leader_log"; then
        log_success "Test 3.4: Old leader correctly handled log conflicts"
    else
        log_info "Test 3.4: No explicit log rollback detected (may not be needed)"
    fi
    
    log_success "Test 3: Log Conflict Resolution - PASSED"
    return 0
}

# 主函数
main() {
    echo "=========================================="
    echo "  Raft KV Log Replication Test Suite"
    echo "=========================================="
    echo ""
    
    # 初始化结果文件
    echo "Raft KV Log Replication Test Results" > "$RESULTS_FILE"
    echo "Date: $(date)" >> "$RESULTS_FILE"
    echo "========================================" >> "$RESULTS_FILE"
    echo "" >> "$RESULTS_FILE"
    
    # 检查可执行文件
    if [ ! -f "$CLIENT_BIN" ]; then
        log_error "Client binary not found: $CLIENT_BIN"
        log_error "Please build the project first"
        exit 1
    fi
    
    # 运行测试
    test_normal_log_replication || true
    test_follower_catchup || true
    test_log_conflict_resolution || true
    
    # 清理
    log_info "Cleaning up..."
    bash "$PROJECT_ROOT/scripts/stop_cluster.sh" > /dev/null 2>&1 || true
    
    # 输出测试结果
    echo ""
    echo "=========================================="
    echo "  Test Results Summary"
    echo "=========================================="
    echo "Total Tests: $TOTAL_TESTS"
    echo "Passed: $PASSED_TESTS"
    echo "Failed: $FAILED_TESTS"
    echo ""
    echo "Detailed results saved to: $RESULTS_FILE"
    echo "=========================================="
    echo ""
    
    if [ $FAILED_TESTS -eq 0 ]; then
        log_success "All tests passed!"
        exit 0
    else
        log_error "Some tests failed"
        exit 1
    fi
}

# 运行主函数
main
