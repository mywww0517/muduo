#!/bin/bash

# 从配置文件加载数据库环境变量
if [ -f "config/db.env" ]; then
    set -a
    source config/db.env
    set +a
else
    echo "错误: config/db.env 文件不存在"
    echo "请创建 config/db.env 文件并设置以下环境变量："
    echo "  CHAT_DB_USER=your_username"
    echo "  CHAT_DB_PASSWORD=your_password"
    echo "  CHAT_DB_NAME=chat"
    echo "  CHAT_DB_HOST=127.0.0.1"
    echo "  CHAT_DB_PORT=3306"
    exit 1
fi

# 启动服务器
PORT=${1:-8888}

echo "Starting ChatServer on port $PORT..."
./build/bin/chatserver $PORT
