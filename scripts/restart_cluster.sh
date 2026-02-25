#!/bin/bash

# Raft KV 集群重启脚本
# 用法: ./restart_cluster.sh [node_count]

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=========================================="
echo "  Restarting Raft KV Cluster"
echo "=========================================="
echo ""

# 停止集群
"$PROJECT_ROOT/scripts/stop_cluster.sh"

echo ""
echo "Waiting for cleanup..."
sleep 2

# 启动集群
"$PROJECT_ROOT/scripts/start_cluster.sh" "$@"
