#!/bin/bash

# Raft KV 集群停止脚本

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PID_DIR="$PROJECT_ROOT/logs"

echo "=========================================="
echo "  Stopping Raft KV Cluster"
echo "=========================================="
echo ""

# 查找所有 PID 文件
PID_FILES=$(find "$PID_DIR" -name "node*.pid" 2>/dev/null || true)

if [ -z "$PID_FILES" ]; then
    echo "No running nodes found"
    exit 0
fi

# 停止所有节点
for PID_FILE in $PID_FILES; do
    NODE_NAME=$(basename "$PID_FILE" .pid)
    PID=$(cat "$PID_FILE" 2>/dev/null || echo "")
    
    if [ -n "$PID" ]; then
        if kill -0 "$PID" 2>/dev/null; then
            echo "Stopping $NODE_NAME (PID: $PID)..."
            kill "$PID" 2>/dev/null || true
            
            # 等待进程结束
            for i in {1..10}; do
                if ! kill -0 "$PID" 2>/dev/null; then
                    break
                fi
                sleep 0.5
            done
            
            # 如果进程仍在运行，强制终止
            if kill -0 "$PID" 2>/dev/null; then
                echo "  Force killing $NODE_NAME..."
                kill -9 "$PID" 2>/dev/null || true
            fi
            
            echo "  $NODE_NAME stopped"
        else
            echo "$NODE_NAME (PID: $PID) is not running"
        fi
    fi
    
    # 删除 PID 文件
    rm -f "$PID_FILE"
done

echo ""
echo "=========================================="
echo "  Cluster stopped successfully!"
echo "=========================================="
echo ""
