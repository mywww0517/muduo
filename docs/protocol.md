# 通信协议文档

## 1. 传输层

- 基于 **TCP** 长连接
- 客户端主动连接服务端，连接建立后可双向通信
- 默认端口：**8888**

## 2. 消息分帧（Message Framing）

TCP 是字节流协议，不保证消息边界。采用 **长度头 + payload** 的方式解决粘包/半包问题。

### 协议格式

    +-------------------+---------------------------+
    | length (4 bytes)  |     JSON payload          |
    +-------------------+---------------------------+
         int32_t              length 个字节

| 字段 | 类型 | 说明 |
|------|------|------|
| length | int32_t (4字节) | payload 的字节数，主机字节序 |
| payload | UTF-8 字符串 | JSON 格式的消息体 |

### 约束

- length 范围：`[0, 65536]`
- 超出范围视为非法数据，服务端断开连接
- payload 必须是合法的 JSON

## 3. 消息格式（JSON）

所有消息的 JSON 体都包含 `msgid` 字段，用于区分消息类型。

### 3.1 PING（心跳检测）

**客户端 → 服务端：**

    {"msgid": 1, "data": "ping"}

**服务端 → 客户端：**

    {"msgid": 2, "data": "pong"}

注册
请求：

{"msgid": 3, "name": "alice", "password": "123456"}
成功响应：

{"msgid": 4, "errno": 0, "id": 1}
失败响应：

{"msgid": 4, "errno": 1, "errmsg": "register failed, name already exists"}
登录
请求：

{"msgid": 5, "id": 1, "password": "123456"}
成功响应：

{"msgid": 6, "errno": 0, "id": 1, "name": "alice"}
密码错误/用户不存在：

{"msgid": 6, "errno": 1, "errmsg": "id or password error"}
重复登录：

{"msgid": 6, "errno": 2, "errmsg": "user already online"}
登出
请求：

{"msgid": 7, "id": 1}

| msgid | 名称 | 方向 | 说明 |
|------:|------|------|------|
| 0 | ERROR | S → C | 错误响应 |
| 1 | PING | C → S | 心跳请求 |
| 2 | PONG | S → C | 心跳响应 |
| 3 | REG | C → S | 注册请求 |
| 4 | REG_ACK | S → C | 注册响应 |
| 5 | LOGIN | C → S | 登录请求 |
| 6 | LOGIN_ACK | S → C | 登录响应 |
| 7 | LOGOUT | C → S | 登出请求 |

## 5. 交互流程

    Client                              Server
      |                                    |
      |  ---- TCP Connect (3-way) ----->   |
      |                                    |
      |  ---- [len][PING JSON] -------->   |
      |                                    |
      |  <--- [len][PONG JSON] ---------   |
      |                                    |
      |  ---- [len][PING JSON] -------->   |
      |                                    |
      |  <--- [len][PONG JSON] ---------   |
      |                                    |
      |  ---- TCP Close (4-way) ------->   |
      |                                    |

## 6. 错误处理

| 情况 | 处理方式 |
|------|---------|
| length 超出 [0, 65536] | 服务端关闭连接 |
| payload 不是合法 JSON | 返回 error 响应 |
| msgid 未定义 | 返回 error 响应 |
| 客户端断开 | 服务端清理连接资源 |













