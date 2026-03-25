#!/bin/bash
set -e

# 测试注册和登录功能
echo "=== 测试注册 ==="
mysql -h127.0.0.1 -uvscode -p'050517Wzx' chat -e "DELETE FROM user WHERE name='testuser';" 2>/dev/null || true

# 启动服务器
bash -c 'set -a; source config/db.env; set +a; ./build/bin/chatserver' &
SERVER_PID=$!
sleep 1

# 测试注册（使用 nc 发送 JSON）
echo '{"msgid":3,"name":"testuser","password":"test123"}' | \
  (echo -ne "\x00\x00\x00\x3a"; cat) | nc 127.0.0.1 8888 > /tmp/reg_resp.bin &
sleep 1

# 检查数据库
echo "=== 检查数据库 ==="
mysql -h127.0.0.1 -uvscode -p'050517Wzx' chat -e "SELECT id, name, LENGTH(password) as pwd_len, state FROM user WHERE name='testuser';" 2>/dev/null

# 清理
kill $SERVER_PID 2>/dev/null || true
echo "=== 测试完成 ==="
