#!/bin/bash
# 监控脚本 - 实时监控集群状态

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)

echo "=========================================="
echo "MyMuduo Chat 集群监控"
echo "=========================================="
echo ""

while true; do
    clear
    echo "=========================================="
    echo "MyMuduo Chat 集群监控"
    echo "时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "=========================================="
    echo ""

    # 检查 Nginx
    echo "[Nginx 状态]"
    if pgrep nginx > /dev/null; then
        echo "  ✓ Nginx 运行中"
        NGINX_CONN=$(netstat -an | grep ":9000" | grep ESTABLISHED | wc -l)
        echo "  当前连接数: ${NGINX_CONN}"
    else
        echo "  ✗ Nginx 未运行"
    fi
    echo ""

    # 检查 ChatServer
    echo "[ChatServer 状态]"
    if pgrep -f "chatserver 8888" > /dev/null; then
        SERVER1_PID=$(pgrep -f "chatserver 8888")
        SERVER1_CONN=$(netstat -an | grep ":8888" | grep ESTABLISHED | wc -l)
        SERVER1_MEM=$(ps -p ${SERVER1_PID} -o rss= | awk '{printf "%.1f MB", $1/1024}')
        echo "  ✓ Server 1 (8888) - PID: ${SERVER1_PID}"
        echo "    连接数: ${SERVER1_CONN}, 内存: ${SERVER1_MEM}"
    else
        echo "  ✗ Server 1 (8888) 未运行"
    fi

    if pgrep -f "chatserver 8889" > /dev/null; then
        SERVER2_PID=$(pgrep -f "chatserver 8889")
        SERVER2_CONN=$(netstat -an | grep ":8889" | grep ESTABLISHED | wc -l)
        SERVER2_MEM=$(ps -p ${SERVER2_PID} -o rss= | awk '{printf "%.1f MB", $1/1024}')
        echo "  ✓ Server 2 (8889) - PID: ${SERVER2_PID}"
        echo "    连接数: ${SERVER2_CONN}, 内存: ${SERVER2_MEM}"
    else
        echo "  ✗ Server 2 (8889) 未运行"
    fi
    echo ""

    # 检查 Redis
    echo "[Redis 状态]"
    if pgrep redis-server > /dev/null; then
        echo "  ✓ Redis 运行中"
    else
        echo "  ✗ Redis 未运行"
    fi
    echo ""

    # 检查 MySQL
    echo "[MySQL 状态]"
    if pgrep mysqld > /dev/null; then
        echo "  ✓ MySQL 运行中"
    else
        echo "  ✗ MySQL 未运行"
    fi
    echo ""

    echo "=========================================="
    echo "按 Ctrl+C 退出监控"
    echo "=========================================="

    sleep 3
done
