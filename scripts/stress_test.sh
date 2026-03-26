#!/bin/bash
# 压力测试脚本 - 测试集群负载均衡和稳定性

set -e

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)

# 测试参数
NGINX_HOST="127.0.0.1"
NGINX_PORT="9000"
CONCURRENT_CLIENTS=50
TEST_DURATION=30  # 秒

echo "=========================================="
echo "MyMuduo Chat 集群压力测试"
echo "=========================================="
echo "目标: ${NGINX_HOST}:${NGINX_PORT}"
echo "并发客户端: ${CONCURRENT_CLIENTS}"
echo "测试时长: ${TEST_DURATION} 秒"
echo "=========================================="
echo ""

# 检查集群是否运行
if ! netstat -tlnp 2>/dev/null | grep -q ":${NGINX_PORT}"; then
    echo "错误: Nginx 未在端口 ${NGINX_PORT} 监听"
    echo "请先启动集群: ./scripts/start_cluster.sh"
    exit 1
fi

# 创建测试目录
TEST_DIR="/tmp/mymuduo-stress-test"
mkdir -p "${TEST_DIR}"

# 清理旧日志
rm -f "${TEST_DIR}"/*.log

echo "开始压力测试..."
echo ""

# 启动并发客户端
for i in $(seq 1 ${CONCURRENT_CLIENTS}); do
    (
        # 发送 PING 消息
        for j in $(seq 1 10); do
            printf '\x00\x00\x00\x0d{"msgid":1}' | nc ${NGINX_HOST} ${NGINX_PORT} -w 1 > /dev/null 2>&1
            sleep 0.1
        done
    ) &
done

# 等待测试完成
sleep ${TEST_DURATION}

# 等待所有后台进程结束
wait

echo ""
echo "压力测试完成！"
echo ""
echo "=========================================="
echo "查看服务器日志："
echo "  Server 1: tail -50 /tmp/chatserver-8888.log"
echo "  Server 2: tail -50 /tmp/chatserver-8889.log"
echo "=========================================="
echo ""
echo "统计连接数："
grep "新的连接" /tmp/chatserver-8888.log 2>/dev/null | wc -l | xargs echo "  Server 1 连接数:"
grep "新的连接" /tmp/chatserver-8889.log 2>/dev/null | wc -l | xargs echo "  Server 2 连接数:"
echo "=========================================="
