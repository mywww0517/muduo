#!/bin/bash

# 启动 ChatServer
# 用法：./run_server.sh [port]

set -e

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
SERVER="${PROJECT_DIR}/build/bin/chatserver"

if [ ! -x "$SERVER" ]; then
    echo "Error: chatserver not found. Run ./build.sh first."
    exit 1
fi

PORT=${1:-8888}
echo "Starting ChatServer on port ${PORT}..."
echo "Press Ctrl+C to stop."
echo ""

"$SERVER"
