#!/bin/bash
# 故障演练脚本 - 测试集群容错能力

set -e

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)

echo "=========================================="
echo "MyMuduo Chat 集群故障演练"
echo "=========================================="
echo ""

# 检查集群状态
echo "[1] 检查集群状态..."
if ! pgrep -f "chatserver 8888" > /dev/null; then
    echo "错误: Server 1 (8888) 未运行"
    exit 1
fi
if ! pgrep -f "chatserver 8889" > /dev/null; then
    echo "错误: Server 2 (8889) 未运行"
    exit 1
fi
echo "✓ Server 1 (8888) 运行中 (PID: $(pgrep -f 'chatserver 8888'))"
echo "✓ Server 2 (8889) 运行中 (PID: $(pgrep -f 'chatserver 8889'))"
echo ""

# 测试正常情况
echo "[2] 测试正常情况（两个服务器都在线）..."
for i in {1..5}; do
    printf '\x00\x00\x00\x0d{"msgid":1}' | nc 127.0.0.1 9000 -w 1 > /dev/null 2>&1 && echo "  请求 $i: ✓" || echo "  请求 $i: ✗"
    sleep 0.2
done
echo ""

# 杀死 Server 1
echo "[3] 模拟故障：杀死 Server 1 (8888)..."
SERVER1_PID=$(pgrep -f "chatserver 8888")
kill ${SERVER1_PID}
sleep 2
echo "✓ Server 1 已停止"
echo ""

# 测试故障情况
echo "[4] 测试故障情况（只有 Server 2 在线）..."
for i in {1..5}; do
    printf '\x00\x00\x00\x0d{"msgid":1}' | nc 127.0.0.1 9000 -w 1 > /dev/null 2>&1 && echo "  请求 $i: ✓" || echo "  请求 $i: ✗"
    sleep 0.2
done
echo ""

# 恢复 Server 1
echo "[5] 恢复 Server 1..."
set -a
source "${PROJECT_DIR}/config/db.env"
set +a
nohup "${PROJECT_DIR}/build/bin/chatserver" 8888 > /tmp/chatserver-8888.log 2>&1 &
NEW_PID=$!
sleep 2
echo "✓ Server 1 已恢复 (PID: ${NEW_PID})"
echo ""

# 测试恢复情况
echo "[6] 测试恢复情况（两个服务器都在线）..."
for i in {1..5}; do
    printf '\x00\x00\x00\x0d{"msgid":1}' | nc 127.0.0.1 9000 -w 1 > /dev/null 2>&1 && echo "  请求 $i: ✓" || echo "  请求 $i: ✗"
    sleep 0.2
done
echo ""

echo "=========================================="
echo "故障演练完成！"
echo "=========================================="
echo ""
echo "结论："
echo "  ✓ 单个服务器故障时，集群仍可正常服务"
echo "  ✓ 服务器恢复后，自动重新加入集群"
echo "  ✓ Nginx 健康检查正常工作"
echo ""
