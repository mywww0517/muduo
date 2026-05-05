#!/bin/bash

# 启动 AI 服务

cd "$(dirname "$0")/../ai_service"

# 检查虚拟环境
if [ ! -d "venv" ]; then
    echo "错误: 虚拟环境不存在，请先运行 python3 -m venv venv && source venv/bin/activate && pip install -r requirements.txt"
    exit 1
fi

# 检查 .env 文件
if [ ! -f ".env" ]; then
    echo "警告: .env 文件不存在，请从 .env.example 复制并配置"
    echo "cp .env.example .env"
    exit 1
fi

# 激活虚拟环境并启动服务
source venv/bin/activate
python app.py
