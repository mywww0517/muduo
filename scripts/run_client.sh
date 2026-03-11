#!/bin/bash

# 启动 ChatClient
# 用法：./run_client.sh [host] [port]

set -e

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CLIENT="${PROJECT_DIR}/build/bin/chatclient"

if [ ! -x "$CLIENT" ]; then
    echo "Error: chatclient not found. Run ./build.sh first."
    exit 1
fi

HOST=${1:-"127.0.0.1"}
PORT=${2:-8888}
echo "Connecting to ${HOST}:${PORT}..."
echo ""

"$CLIENT"
