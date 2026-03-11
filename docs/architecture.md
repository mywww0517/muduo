# 系统架构

## 整体架构

    ┌──────────┐          TCP           ┌──────────┐
    │  Client  │ ◄─────────────────────►│  Server  │
    │          │    [len][JSON payload]  │          │
    └──────────┘                        └──────────┘

## Server 内部架构

    新连接 ──→ onConnection()       记录/清理连接
    原始字节 → codecOnMessage()     [len][payload] → JSON string，处理粘包/半包
    JSON str → onJsonMessage()      解析 JSON，switch(msgid) 分发处理
    响应    → codecSend()           JSON string → [len][payload]

## 线程模型

### Server

    Main Loop Thread (1个)  ← 只做 accept 新连接
         │ 分发
         ├── IO Loop Thread 1  ← 处理读写
         └── IO Loop Thread 2  ← 处理读写

### Client

    Main Thread           IO Loop Thread
    getline(用户输入)      EventLoop
         │ send ────────→ onMessage 回调
         │                处理网络读写

## 数据流

### 发送方

    JSON object
      → json.dump() 序列化为 string
      → codecSend() 加 4 字节长度头
      → TcpConnection::send() 写入 socket

### 接收方

    socket 数据到达
      → muduo 读入 Buffer
      → codecOnMessage() 检查长度、提取完整消息
      → onJsonMessage() 解析 JSON
      → 业务处理