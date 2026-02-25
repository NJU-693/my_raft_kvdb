#!/bin/bash

# Raft KV 集群状态检查脚本

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PID_DIR="$PROJECT_ROOT/logs"

echo "=========================================="
echo "  Raft KV Cluster Status"
echo "=========================================="
echo ""

# 查找所有 PID 文件
PID_FILES=$(find "$PID_DIR" -name "node*.pid" 2>/dev/null | sort || true)

if [ -z "$PID_FILES" ]; then
    echo "No nodes configured"
    exit 0
fi

RUNNING_COUNT=0
TOTAL_COUNT=0

for PID_FILE in $PID_FILES; do
    NODE_NAME=$(basename "$PID_FILE" .pid)
    PID=$(cat "$PID_FILE" 2>/dev/null || echo "")
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    
    if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
        echo "✓ $NODE_NAME (PID: $PID) - RUNNING"
        RUNNING_COUNT=$((RUNNING_COUNT + 1))
    else
        echo "✗ $NODE_NAME - STOPPED"
    fi
done

echo ""
echo "Summary: $RUNNING_COUNT/$TOTAL_COUNT nodes running"
echo ""

# 显示最近的日志
if [ $RUNNING_COUNT -gt 0 ]; then
    echo "Recent logs (last 5 lines from each node):"
    echo "=========================================="
    for PID_FILE in $PID_FILES; do
        NODE_NAME=$(basename "$PID_FILE" .pid)
        LOG_FILE="$PID_DIR/$NODE_NAME.log"
        
        if [ -f "$LOG_FILE" ]; then
            echo ""
            echo "--- $NODE_NAME ---"
            tail -n 5 "$LOG_FILE" 2>/dev/null || echo "No logs available"
        fi
    done
fi

echo ""
