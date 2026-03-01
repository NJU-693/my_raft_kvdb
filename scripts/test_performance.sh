#!/bin/bash

# Raft KV 性能测试脚本
# 测试系统的 QPS（每秒查询数）

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
CYAN='\033[0;36m'
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

log_result() {
    echo -e "${CYAN}[RESULT]${NC} $1"
}

# 检查集群是否运行
check_cluster() {
    local running=0
    for i in {1..3}; do
        local pid_file="$PROJECT_ROOT/logs/node$i.pid"
        if [ -f "$pid_file" ]; then
            local pid=$(cat "$pid_file")
            if kill -0 "$pid" 2>/dev/null; then
                running=$((running + 1))
            fi
        fi
    done
    echo $running
}

# 测试写入性能
test_write_performance() {
    local operation_count=$1
    local test_name=$2
    
    log_info "Testing: $test_name"
    log_info "Operations: $operation_count"
    
    local start_time=$(date +%s.%N)
    local success_count=0
    local fail_count=0
    
    for i in $(seq 1 $operation_count); do
        if $CLIENT_BIN PUT "perf_key_$i" "value_$i" > /dev/null 2>&1; then
            success_count=$((success_count + 1))
        else
            fail_count=$((fail_count + 1))
        fi
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(awk "BEGIN {print $end_time - $start_time}")
    local qps=$(awk "BEGIN {printf \"%.2f\", $success_count / $duration}")
    local avg_latency=$(awk "BEGIN {printf \"%.2f\", $duration * 1000 / $success_count}")
    
    log_result "Duration: ${duration}s"
    log_result "Success: $success_count, Failed: $fail_count"
    log_result "QPS: $qps ops/sec"
    log_result "Average Latency: ${avg_latency}ms"
    
    # 返回纯数字，不带颜色代码
    printf "%.2f" "$qps"
}

# 测试读取性能
test_read_performance() {
    local operation_count=$1
    local test_name=$2
    
    log_info "Testing: $test_name"
    log_info "Operations: $operation_count"
    
    local start_time=$(date +%s.%N)
    local success_count=0
    local fail_count=0
    
    for i in $(seq 1 $operation_count); do
        if $CLIENT_BIN GET "perf_key_$i" > /dev/null 2>&1; then
            success_count=$((success_count + 1))
        else
            fail_count=$((fail_count + 1))
        fi
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(awk "BEGIN {print $end_time - $start_time}")
    local qps=$(awk "BEGIN {printf \"%.2f\", $success_count / $duration}")
    local avg_latency=$(awk "BEGIN {printf \"%.2f\", $duration * 1000 / $success_count}")
    
    log_result "Duration: ${duration}s"
    log_result "Success: $success_count, Failed: $fail_count"
    log_result "QPS: $qps ops/sec"
    log_result "Average Latency: ${avg_latency}ms"
    
    # 返回纯数字，不带颜色代码
    printf "%.2f" "$qps"
}

# 测试混合性能（读写混合）
test_mixed_performance() {
    local operation_count=$1
    local test_name=$2
    
    log_info "Testing: $test_name"
    log_info "Operations: $operation_count (50% write, 50% read)"
    
    local start_time=$(date +%s.%N)
    local success_count=0
    local fail_count=0
    
    for i in $(seq 1 $operation_count); do
        if [ $((i % 2)) -eq 0 ]; then
            # 写操作
            if $CLIENT_BIN PUT "mixed_key_$i" "value_$i" > /dev/null 2>&1; then
                success_count=$((success_count + 1))
            else
                fail_count=$((fail_count + 1))
            fi
        else
            # 读操作
            if $CLIENT_BIN GET "perf_key_$((i/2 + 1))" > /dev/null 2>&1; then
                success_count=$((success_count + 1))
            else
                fail_count=$((fail_count + 1))
            fi
        fi
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(awk "BEGIN {print $end_time - $start_time}")
    local qps=$(awk "BEGIN {printf \"%.2f\", $success_count / $duration}")
    local avg_latency=$(awk "BEGIN {printf \"%.2f\", $duration * 1000 / $success_count}")
    
    log_result "Duration: ${duration}s"
    log_result "Success: $success_count, Failed: $fail_count"
    log_result "QPS: $qps ops/sec"
    log_result "Average Latency: ${avg_latency}ms"
    
    # 返回纯数字，不带颜色代码
    printf "%.2f" "$qps"
}

# 并发性能测试
test_concurrent_performance() {
    local clients=$1
    local ops_per_client=$2
    local test_name=$3
    
    log_info "Testing: $test_name"
    log_info "Concurrent Clients: $clients"
    log_info "Operations per Client: $ops_per_client"
    log_info "Total Operations: $((clients * ops_per_client))"
    
    local start_time=$(date +%s.%N)
    
    # 启动多个并发客户端
    local pids=()
    for c in $(seq 1 $clients); do
        (
            for i in $(seq 1 $ops_per_client); do
                $CLIENT_BIN PUT "concurrent_${c}_${i}" "value_${i}" > /dev/null 2>&1
            done
        ) &
        pids+=($!)
    done
    
    # 等待所有客户端完成
    for pid in "${pids[@]}"; do
        wait $pid
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(awk "BEGIN {print $end_time - $start_time}")
    local total_ops=$((clients * ops_per_client))
    local qps=$(awk "BEGIN {printf \"%.2f\", $total_ops / $duration}")
    local avg_latency=$(awk "BEGIN {printf \"%.2f\", $duration * 1000 / $total_ops}")
    
    log_result "Duration: ${duration}s"
    log_result "Total Operations: $total_ops"
    log_result "QPS: $qps ops/sec"
    log_result "Average Latency: ${avg_latency}ms"
    
    # 返回纯数字，不带颜色代码
    printf "%.2f" "$qps"
}

echo "=========================================="
echo "  Raft KV Performance Test"
echo "=========================================="
echo ""

# 检查可执行文件
if [ ! -f "$CLIENT_BIN" ]; then
    log_error "Client binary not found: $CLIENT_BIN"
    log_error "Please build the project first"
    exit 1
fi

# 检查集群状态
log_info "Checking cluster status..."
running_nodes=$(check_cluster)

if [ $running_nodes -lt 3 ]; then
    log_warning "Only $running_nodes nodes running. Starting cluster..."
    bash "$PROJECT_ROOT/scripts/stop_cluster.sh" > /dev/null 2>&1 || true
    sleep 2
    
    # 清理数据
    rm -rf "$PROJECT_ROOT/data/node"*
    
    bash "$PROJECT_ROOT/scripts/start_cluster.sh" 3 > /dev/null 2>&1
    sleep 5
    log_success "Cluster started"
else
    log_success "Cluster is running ($running_nodes nodes)"
fi

echo ""

# 测试 1: 顺序写入性能（小数据集）
echo "=========================================="
echo "  Test 1: Sequential Write (Small)"
echo "=========================================="
write_qps_small=$(test_write_performance 50 "Sequential Write - 50 operations")
echo ""

# 测试 2: 顺序写入性能（中等数据集）
echo "=========================================="
echo "  Test 2: Sequential Write (Medium)"
echo "=========================================="
write_qps_medium=$(test_write_performance 100 "Sequential Write - 100 operations")
echo ""

# 测试 3: 顺序读取性能
echo "=========================================="
echo "  Test 3: Sequential Read"
echo "=========================================="
read_qps=$(test_read_performance 100 "Sequential Read - 100 operations")
echo ""

# 测试 4: 混合读写性能
echo "=========================================="
echo "  Test 4: Mixed Read/Write"
echo "=========================================="
mixed_qps=$(test_mixed_performance 100 "Mixed Operations - 100 operations")
echo ""

# 测试 5: 并发写入性能（3 客户端）
echo "=========================================="
echo "  Test 5: Concurrent Write (3 clients)"
echo "=========================================="
concurrent_qps_3=$(test_concurrent_performance 3 20 "Concurrent Write - 3 clients x 20 ops")
echo ""

# 测试 6: 并发写入性能（5 客户端）
echo "=========================================="
echo "  Test 6: Concurrent Write (5 clients)"
echo "=========================================="
concurrent_qps_5=$(test_concurrent_performance 5 20 "Concurrent Write - 5 clients x 20 ops")
echo ""

# 测试 7: 高并发写入性能（10 客户端）
echo "=========================================="
echo "  Test 7: Concurrent Write (10 clients)"
echo "=========================================="
concurrent_qps_10=$(test_concurrent_performance 10 10 "Concurrent Write - 10 clients x 10 ops")
echo ""

# 汇总结果
echo "=========================================="
echo "  Performance Summary"
echo "=========================================="
echo ""
echo "Sequential Performance:"
echo "  Write (50 ops):    $write_qps_small ops/sec"
echo "  Write (100 ops):   $write_qps_medium ops/sec"
echo "  Read (100 ops):    $read_qps ops/sec"
echo "  Mixed (100 ops):   $mixed_qps ops/sec"
echo ""
echo "Concurrent Performance:"
echo "  3 clients:         $concurrent_qps_3 ops/sec"
echo "  5 clients:         $concurrent_qps_5 ops/sec"
echo "  10 clients:        $concurrent_qps_10 ops/sec"
echo ""

# 计算平均 QPS
avg_write_qps=$(awk "BEGIN {printf \"%.2f\", ($write_qps_small + $write_qps_medium) / 2}")
avg_concurrent_qps=$(awk "BEGIN {printf \"%.2f\", ($concurrent_qps_3 + $concurrent_qps_5 + $concurrent_qps_10) / 3}")

echo "Average Performance:"
echo "  Sequential Write:  $avg_write_qps ops/sec"
echo "  Sequential Read:   $read_qps ops/sec"
echo "  Concurrent Write:  $avg_concurrent_qps ops/sec"
echo ""

# 性能评估
log_info "Performance Assessment:"
if (( $(awk "BEGIN {print ($avg_write_qps > 50)}") )); then
    log_success "Write performance: EXCELLENT (>50 ops/sec)"
elif (( $(awk "BEGIN {print ($avg_write_qps > 20)}") )); then
    log_success "Write performance: GOOD (>20 ops/sec)"
elif (( $(awk "BEGIN {print ($avg_write_qps > 10)}") )); then
    log_warning "Write performance: ACCEPTABLE (>10 ops/sec)"
else
    log_warning "Write performance: NEEDS IMPROVEMENT (<10 ops/sec)"
fi

if (( $(awk "BEGIN {print ($read_qps > 100)}") )); then
    log_success "Read performance: EXCELLENT (>100 ops/sec)"
elif (( $(awk "BEGIN {print ($read_qps > 50)}") )); then
    log_success "Read performance: GOOD (>50 ops/sec)"
elif (( $(awk "BEGIN {print ($read_qps > 20)}") )); then
    log_warning "Read performance: ACCEPTABLE (>20 ops/sec)"
else
    log_warning "Read performance: NEEDS IMPROVEMENT (<20 ops/sec)"
fi

if (( $(awk "BEGIN {print ($avg_concurrent_qps > 100)}") )); then
    log_success "Concurrent performance: EXCELLENT (>100 ops/sec)"
elif (( $(awk "BEGIN {print ($avg_concurrent_qps > 50)}") )); then
    log_success "Concurrent performance: GOOD (>50 ops/sec)"
elif (( $(awk "BEGIN {print ($avg_concurrent_qps > 20)}") )); then
    log_warning "Concurrent performance: ACCEPTABLE (>20 ops/sec)"
else
    log_warning "Concurrent performance: NEEDS IMPROVEMENT (<20 ops/sec)"
fi

echo ""
echo "=========================================="
log_success "Performance test completed!"
echo "=========================================="
echo ""
echo "Note: Performance depends on hardware, network latency,"
echo "      and system load. These results are for reference only."
echo ""
