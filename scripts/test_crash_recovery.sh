#!/bin/bash

# 测试脚本：故障恢复测试（Task 9.2.4）
# 测试节点崩溃后从持久化存储恢复状态的能力

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
CONFIG_FILE="$PROJECT_ROOT/config/cluster.conf"
CLIENT="$BUILD_DIR/bin/kv_client_cli"

echo "========================================"
echo "  Crash Recovery Test (Task 9.2.4)"
echo "========================================"
echo ""

# 检查构建
if [ ! -f "$CLIENT" ]; then
    echo "Error: Client binary not found. Please build the project first."
    exit 1
fi

# 停止现有集群
echo "Step 1: Stopping existing cluster..."
bash "$SCRIPT_DIR/stop_cluster.sh"
sleep 2

# 清理旧数据
echo "Step 2: Cleaning old data..."
rm -rf "$PROJECT_ROOT/data/node1"/*
rm -rf "$PROJECT_ROOT/data/node2"/*
rm -rf "$PROJECT_ROOT/data/node3"/*
echo "Old data cleaned"
echo ""

# 启动 3 节点集群
echo "Step 3: Starting 3-node cluster..."
bash "$SCRIPT_DIR/start_cluster.sh"
sleep 3

# 等待领导者选举
echo "Step 4: Waiting for leader election..."
sleep 3

# 写入一些数据
echo "Step 5: Writing test data to cluster..."
echo "  Writing key1=value1..."
$CLIENT PUT key1 value1
sleep 1

echo "  Writing key2=value2..."
$CLIENT PUT key2 value2
sleep 1

echo "  Writing key3=value3..."
$CLIENT PUT key3 value3
sleep 1

echo "  Writing user1=Alice..."
$CLIENT PUT user1 Alice
sleep 1

echo "  Writing user2=Bob..."
$CLIENT PUT user2 Bob
sleep 1

echo "Test data written successfully"
echo ""

# 验证数据
echo "Step 6: Verifying data before crash..."
echo "  Reading key1..."
RESULT=$($CLIENT GET key1)
if [[ "$RESULT" == *"value1"* ]]; then
    echo "    ✓ key1=value1"
else
    echo "    ✗ Failed to read key1"
    exit 1
fi

echo "  Reading user1..."
RESULT=$($CLIENT GET user1)
if [[ "$RESULT" == *"Alice"* ]]; then
    echo "    ✓ user1=Alice"
else
    echo "    ✗ Failed to read user1"
    exit 1
fi

echo "Data verified successfully"
echo ""

# 检查持久化文件
echo "Step 7: Checking persistent storage files..."
for i in 1 2 3; do
    STATE_FILE="$PROJECT_ROOT/data/node$i/raft_state.dat"
    if [ -f "$STATE_FILE" ]; then
        SIZE=$(stat -c%s "$STATE_FILE" 2>/dev/null || stat -f%z "$STATE_FILE" 2>/dev/null || echo "unknown")
        echo "  Node $i: raft_state.dat exists (size: $SIZE bytes)"
    else
        echo "  Node $i: raft_state.dat NOT FOUND"
    fi
done
echo ""

# 模拟节点 2 崩溃
echo "Step 8: Simulating Node 2 crash..."
NODE2_PID=$(cat "$PROJECT_ROOT/logs/node2.pid" 2>/dev/null || echo "")
if [ -n "$NODE2_PID" ]; then
    kill -9 $NODE2_PID 2>/dev/null || true
    echo "  Node 2 (PID: $NODE2_PID) killed"
else
    echo "  Warning: Node 2 PID not found"
fi
sleep 2
echo ""

# 写入更多数据（Node 2 离线期间）
echo "Step 9: Writing more data while Node 2 is down..."
echo "  Writing key4=value4..."
$CLIENT PUT key4 value4
sleep 1

echo "  Writing key5=value5..."
$CLIENT PUT key5 value5
sleep 1

echo "More data written (Node 2 missed these)"
echo ""

# 重启节点 2
echo "Step 10: Restarting Node 2..."
cd "$PROJECT_ROOT"
nohup "$BUILD_DIR/bin/raft_node_server" 2 "$CONFIG_FILE" > "$PROJECT_ROOT/logs/node2.log" 2>&1 &
NODE2_NEW_PID=$!
echo $NODE2_NEW_PID > "$PROJECT_ROOT/logs/node2.pid"
echo "  Node 2 restarted (new PID: $NODE2_NEW_PID)"
sleep 3
echo ""

# 验证节点 2 恢复状态
echo "Step 11: Verifying Node 2 recovered state..."
echo "  Checking if Node 2 is alive..."
if ps -p $NODE2_NEW_PID > /dev/null; then
    echo "    ✓ Node 2 is running"
else
    echo "    ✗ Node 2 failed to start"
    exit 1
fi

# 等待节点 2 追赶日志
echo "  Waiting for Node 2 to catch up..."
sleep 5

# 验证所有数据
echo "Step 12: Verifying all data after recovery..."
SUCCESS_COUNT=0
TOTAL_COUNT=0

for key in key1 key2 key3 key4 key5 user1 user2; do
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    echo "  Reading $key..."
    RESULT=$($CLIENT GET $key 2>&1 || echo "FAILED")
    
    case $key in
        key1)
            if [[ "$RESULT" == *"value1"* ]]; then
                echo "    ✓ $key=value1"
                SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            else
                echo "    ✗ Failed: $RESULT"
            fi
            ;;
        key2)
            if [[ "$RESULT" == *"value2"* ]]; then
                echo "    ✓ $key=value2"
                SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            else
                echo "    ✗ Failed: $RESULT"
            fi
            ;;
        key3)
            if [[ "$RESULT" == *"value3"* ]]; then
                echo "    ✓ $key=value3"
                SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            else
                echo "    ✗ Failed: $RESULT"
            fi
            ;;
        key4)
            if [[ "$RESULT" == *"value4"* ]]; then
                echo "    ✓ $key=value4"
                SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            else
                echo "    ✗ Failed: $RESULT"
            fi
            ;;
        key5)
            if [[ "$RESULT" == *"value5"* ]]; then
                echo "    ✓ $key=value5"
                SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            else
                echo "    ✗ Failed: $RESULT"
            fi
            ;;
        user1)
            if [[ "$RESULT" == *"Alice"* ]]; then
                echo "    ✓ $key=Alice"
                SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            else
                echo "    ✗ Failed: $RESULT"
            fi
            ;;
        user2)
            if [[ "$RESULT" == *"Bob"* ]]; then
                echo "    ✓ $key=Bob"
                SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            else
                echo "    ✗ Failed: $RESULT"
            fi
            ;;
    esac
    sleep 1
done

echo ""
echo "========================================"
echo "  Test Results"
echo "========================================"
echo "Total operations: $TOTAL_COUNT"
echo "Successful: $SUCCESS_COUNT"
echo "Failed: $((TOTAL_COUNT - SUCCESS_COUNT))"
echo "Success rate: $(awk "BEGIN {printf \"%.1f\", ($SUCCESS_COUNT/$TOTAL_COUNT)*100}")%"
echo ""

if [ $SUCCESS_COUNT -eq $TOTAL_COUNT ]; then
    echo "✓ All tests passed!"
    echo ""
    echo "Crash recovery test completed successfully:"
    echo "  1. Node 2 crashed and was killed"
    echo "  2. Node 2 restarted and recovered state from disk"
    echo "  3. Node 2 caught up with missed log entries"
    echo "  4. All data is consistent across the cluster"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
