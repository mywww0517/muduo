#!/bin/bash
# 停止 MyMuduo Chat 集群

echo "Stopping ChatServer instances..."

pkill -f "chatserver 8888"
pkill -f "chatserver 8889"

sleep 1

echo "Cluster stopped."
echo ""
echo "Check remaining processes: ps aux | grep chatserver"
