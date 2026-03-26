#!/bin/bash
# 启动 MyMuduo Chat 集群（带 Nginx 负载均衡）

set -e

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)

# 加载数据库配置
set -a
source "${PROJECT_DIR}/config/db.env"
set +a

# 检查 nginx 配置
if ! sudo nginx -t > /dev/null 2>&1; then
    echo "Error: Nginx configuration test failed"
    exit 1
fi

# 启动后端服务器
echo "Starting ChatServer instances..."

nohup "${PROJECT_DIR}/build/bin/chatserver" 8888 > /tmp/chatserver-8888.log 2>&1 &
SERVER1_PID=$!
echo "ChatServer 1 started on port 8888 (PID: $SERVER1_PID)"

sleep 1

nohup "${PROJECT_DIR}/build/bin/chatserver" 8889 > /tmp/chatserver-8889.log 2>&1 &
SERVER2_PID=$!
echo "ChatServer 2 started on port 8889 (PID: $SERVER2_PID)"

sleep 1

# 重载 nginx
echo "Reloading Nginx..."
sudo systemctl reload nginx

echo ""
echo "Cluster started successfully!"
echo "  - ChatServer 1: 127.0.0.1:8888 (PID: $SERVER1_PID)"
echo "  - ChatServer 2: 127.0.0.1:8889 (PID: $SERVER2_PID)"
echo "  - Nginx Load Balancer: 127.0.0.1:9000"
echo ""
echo "Connect to: ./build/bin/chatclient 127.0.0.1 9000"
echo ""
echo "Logs:"
echo "  - Server 1: tail -f /tmp/chatserver-8888.log"
echo "  - Server 2: tail -f /tmp/chatserver-8889.log"
