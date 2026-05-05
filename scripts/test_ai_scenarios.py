#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
v2.0 AI 功能多场景测试
"""

import socket
import json
import struct
import time

def encode_message(msg_dict):
    payload = json.dumps(msg_dict, ensure_ascii=False).encode('utf-8')
    length = len(payload)
    return struct.pack('i', length) + payload

def decode_message(sock):
    try:
        length_data = sock.recv(4)
        if len(length_data) < 4:
            return None
        length = struct.unpack('i', length_data)[0]
        payload = b''
        while len(payload) < length:
            chunk = sock.recv(length - len(payload))
            if not chunk:
                return None
            payload += chunk
        return json.loads(payload.decode('utf-8'))
    except Exception as e:
        return None

print("=" * 70)
print("v2.0 AI 功能多场景测试")
print("=" * 70)

# 连接并登录
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8888))
print("\n✅ 已连接到服务器")

login_msg = {"msgid": 5, "id": 1, "password": "123456"}
sock.send(encode_message(login_msg))
sock.settimeout(3)
response = decode_message(sock)

if response and response.get('errno') == 0:
    print(f"✅ 登录成功: {response.get('name')}\n")
else:
    print("❌ 登录失败")
    exit(1)

# 测试多个场景
test_cases = [
    "@AI 测试一下系统",
    "@AI 请介绍一下你自己",
    "@AI 帮助",
]

for i, test_msg in enumerate(test_cases, 1):
    print(f"[测试 {i}] 发送: {test_msg}")

    group_msg = {
        "msgid": 34,
        "id": 1,
        "name": "bob",
        "groupid": 1,
        "msg": test_msg,
        "time": int(time.time())
    }

    sock.send(encode_message(group_msg))

    # 等待 AI 回复
    sock.settimeout(10)
    try:
        msg = decode_message(sock)
        if msg and msg.get('msgid') == 70:
            print(f"✅ AI 回复: {msg.get('msg')}\n")
        else:
            print(f"⚠️  收到其他消息: {msg}\n")
    except socket.timeout:
        print("⏱️  超时\n")

    time.sleep(1)

sock.close()
print("=" * 70)
print("✅ 所有测试完成")
print("=" * 70)
