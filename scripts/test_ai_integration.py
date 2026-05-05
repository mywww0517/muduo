#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
v2.0 AI 功能集成测试脚本
模拟客户端发送 @AI 消息，验证 AI 回复
"""

import socket
import json
import struct
import time
import sys

def encode_message(msg_dict):
    """编码消息：[4字节长度][JSON payload]"""
    payload = json.dumps(msg_dict, ensure_ascii=False).encode('utf-8')
    length = len(payload)
    return struct.pack('!I', length) + payload

def decode_message(sock):
    """解码消息"""
    # 读取 4 字节长度头
    length_data = sock.recv(4)
    if len(length_data) < 4:
        return None

    length = struct.unpack('!I', length_data)[0]

    # 读取 JSON payload
    payload = b''
    while len(payload) < length:
        chunk = sock.recv(length - len(payload))
        if not chunk:
            return None
        payload += chunk

    return json.loads(payload.decode('utf-8'))

def test_ai_integration():
    """测试 AI 集成功能"""

    print("=" * 60)
    print("v2.0 AI 功能集成测试")
    print("=" * 60)

    # 连接服务器
    print("\n[1] 连接 ChatServer...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(('127.0.0.1', 8888))
        print("✅ 连接成功")
    except Exception as e:
        print(f"❌ 连接失败: {e}")
        return False

    # 登录用户
    print("\n[2] 登录用户...")
    login_msg = {
        "msgid": 2,  # LOGIN_MSG
        "id": 1,
        "password": "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92"  # SHA256 of "123456"
    }

    sock.send(encode_message(login_msg))
    print("⏳ 等待登录响应...")

    sock.settimeout(5)
    try:
        response = decode_message(sock)
    except socket.timeout:
        print("❌ 登录超时")
        return False

    print(f"📥 收到响应: {response}")

    if response and response.get('msgid') == 3:  # LOGIN_ACK
        if response.get('errno') == 0:
            print(f"✅ 登录成功: {response.get('name')}")
        else:
            print(f"❌ 登录失败: {response.get('errmsg')}")
            return False
    else:
        print(f"❌ 登录响应异常: {response}")
        return False

    # 查询群组列表
    groups = response.get('groups', [])
    if not groups:
        print("\n⚠️  用户没有加入任何群组，请先创建或加入群组")
        print("提示：使用客户端执行 creategroup 或 joingroup")
        return False

    groupid = groups[0]['id']
    groupname = groups[0]['groupname']
    print(f"\n[3] 使用群组: {groupname} (ID: {groupid})")

    # 发送 @AI 消息
    print("\n[4] 发送 @AI 消息...")
    test_messages = [
        "@AI 你好",
        "@AI 测试一下系统",
        "@AI 请介绍一下你自己"
    ]

    for msg_text in test_messages:
        print(f"\n发送: {msg_text}")

        group_msg = {
            "msgid": 9,  # GROUP_CHAT_MSG
            "id": 1,
            "name": "test_user",
            "groupid": groupid,
            "msg": msg_text,
            "time": int(time.time())
        }

        sock.send(encode_message(group_msg))
        print("✅ 消息已发送")

        # 等待 AI 回复
        print("⏳ 等待 AI 回复...")
        sock.settimeout(10)

        try:
            ai_response = decode_message(sock)
            if ai_response:
                if ai_response.get('msgid') == 70:  # AI_CHAT_MSG
                    print(f"✅ 收到 AI 回复:")
                    print(f"   发送者: {ai_response.get('name')} (ID: {ai_response.get('id')})")
                    print(f"   内容: {ai_response.get('msg')}")
                else:
                    print(f"⚠️  收到其他消息: msgid={ai_response.get('msgid')}")
            else:
                print("❌ 未收到响应")
        except socket.timeout:
            print("⏱️  等待超时（AI 服务可能未启动或响应慢）")

        time.sleep(2)

    # 关闭连接
    sock.close()
    print("\n" + "=" * 60)
    print("测试完成")
    print("=" * 60)

    return True

if __name__ == '__main__':
    try:
        success = test_ai_integration()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("\n\n测试中断")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ 测试异常: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
