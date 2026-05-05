#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
简化版 AI 集成测试
直接发送群聊消息测试 @AI 功能
"""

import socket
import json
import struct
import time

def encode_message(msg_dict):
    """编码消息：[4字节长度][JSON payload]"""
    payload = json.dumps(msg_dict, ensure_ascii=False).encode('utf-8')
    length = len(payload)
    # 使用主机字节序（小端），不是网络字节序
    return struct.pack('i', length) + payload

def decode_message(sock):
    """解码消息"""
    try:
        # 读取 4 字节长度头（主机字节序）
        length_data = sock.recv(4)
        if len(length_data) < 4:
            return None

        length = struct.unpack('i', length_data)[0]

        # 读取 JSON payload
        payload = b''
        while len(payload) < length:
            chunk = sock.recv(length - len(payload))
            if not chunk:
                return None
            payload += chunk

        return json.loads(payload.decode('utf-8'))
    except Exception as e:
        print(f"解码错误: {e}")
        return None

print("=" * 70)
print("v2.0 AI 功能集成测试 - 简化版")
print("=" * 70)

# 连接服务器
print("\n[1] 连接 ChatServer (127.0.0.1:8888)...")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8888))
print("✅ 连接成功")

# 登录
print("\n[2] 登录用户 bob (id=1)...")
login_msg = {
    "msgid": 5,  # LOGIN_MSG
    "id": 1,
    "password": "123456"  # 明文密码
}
sock.send(encode_message(login_msg))

sock.settimeout(3)
response = decode_message(sock)

if response and response.get('errno') == 0:
    print(f"✅ 登录成功: {response.get('name')}")
    groups = response.get('groups', [])
    if groups:
        group_names = [f"{g.get('groupname', g.get('name', 'unknown'))}(id={g['id']})" for g in groups]
        print(f"   群组列表: {group_names}")
    else:
        print("   群组列表: 无")
else:
    print(f"❌ 登录失败: {response}")
    exit(1)

# 发送 @AI 消息
print("\n[3] 发送 @AI 消息到群组 1...")
test_msg = "@AI 你好，请介绍一下你自己"
print(f"   消息内容: {test_msg}")

group_msg = {
    "msgid": 34,  # GROUP_CHAT_MSG
    "id": 1,
    "name": "bob",
    "groupid": 1,
    "msg": test_msg,
    "time": int(time.time())
}

sock.send(encode_message(group_msg))
print("✅ 消息已发送")

# 等待 AI 回复
print("\n[4] 等待 AI 回复...")
sock.settimeout(15)

start_time = time.time()
ai_replied = False

while time.time() - start_time < 15:
    try:
        msg = decode_message(sock)
        if msg:
            msgid = msg.get('msgid')

            if msgid == 70:  # AI_CHAT_MSG
                print(f"\n✅ 收到 AI 回复!")
                print(f"   发送者: {msg.get('name')} (ID: {msg.get('id')})")
                print(f"   群组: {msg.get('groupid')}")
                print(f"   内容: {msg.get('msg')}")
                ai_replied = True
                break
            elif msgid == 9:  # GROUP_CHAT_MSG
                print(f"   [群消息] {msg.get('name')}: {msg.get('msg')[:50]}")
            else:
                print(f"   [其他消息] msgid={msgid}")
    except socket.timeout:
        print("⏱️  等待超时")
        break
    except Exception as e:
        print(f"❌ 接收错误: {e}")
        break

sock.close()

print("\n" + "=" * 70)
if ai_replied:
    print("✅ 测试成功！AI 功能正常工作")
else:
    print("❌ 测试失败：未收到 AI 回复")
    print("\n可能的原因：")
    print("  1. AI 服务未启动（检查: curl http://127.0.0.1:5000/api/health）")
    print("  2. ChatServer 未正确调用 AI 服务")
    print("  3. 网络超时")
print("=" * 70)
