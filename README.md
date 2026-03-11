# MyMuduo Chat

基于 **muduo** 网络库的轻量级聊天服务器，使用 C++11 实现。

## 项目特点

- 基于 muduo 的 Reactor 模型（epoll + 非阻塞 IO）
- one loop per thread 线程模型
- JSON 协议通信（nlohmann/json）
- 自定义 codec 解决 TCP 粘包/半包问题
- 长度头 + payload 的消息分帧机制

## 架构

```text
┌────────────────────────────────────────────┐
│              Application Layer             │
│         ChatServer / ChatClient            │
├────────────────────────────────────────────┤
│              Protocol Layer                │
│    codec (length-prefix framing)           │
│    JSON serialization/deserialization      │
├────────────────────────────────────────────┤
│              Network Layer                 │
│    muduo (TcpServer/TcpClient/EventLoop)   │
├────────────────────────────────────────────┤
│              System Layer                  │
│    Linux (epoll/socket/pthread)            │
└────────────────────────────────────────────┘
```

## 目录结构

```text
mymuduo/
├── CMakeLists.txt              # 顶层 CMake（统一管理所有 target）
├── README.md
├── scripts/
│   ├── build.sh                # 一键编译脚本
│   ├── run_server.sh           # 启动服务端
│   └── run_client.sh           # 启动客户端
├── thirdparty/
│   └── json.hpp                # nlohmann/json 单头文件
├── include/                    # 预留的公共头文件目录
├── src/
│   ├── common/
│   │   ├── protocol.hpp        # 消息 ID 定义
│   │   ├── json_helper.hpp     # JSON 序列化工具
│   │   └── codec.hpp           # 编解码器（粘包/半包处理）
│   ├── server/
│   │   ├── chatserver.cpp      # 聊天服务端
│   │   └── echoserver.cpp      # 回声服务端（学习用）
│   └── client/
│       ├── chatclient.cpp      # 聊天客户端
│       ├── echoclient.cpp      # 回声客户端（学习用）
│       └── stresstest.cpp      # 粘包压力测试
├── docs/
│   ├── protocol.md             # 通信协议文档
│   └── architecture.md         # 架构文档
└── build/                      # 编译产物（不提交到 git）
    └── bin/
        ├── chatserver
        ├── chatclient
        ├── echoserver
        ├── echoclient
        └── stresstest
```

## 环境要求

- Linux（Ubuntu 18.04+）
- GCC 7+（支持 C++11）
- CMake 3.10+
- muduo 网络库

## 安装 muduo

```bash
sudo apt-get install libboost-all-dev
git clone https://github.com/chenshuo/muduo.git
cd muduo
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build -j$(nproc)
sudo cmake --install build
```

## 快速开始

### 编译

```bash
git clone https://github.com/mywww0517/muduo.git
cd muduo
./scripts/build.sh
```

### 运行

**终端 1：启动服务端**

```bash
./scripts/run_server.sh
# 或者
./build/bin/chatserver
```

**终端 2：启动客户端**

```bash
./scripts/run_client.sh
# 或者
./build/bin/chatclient
```

### 客户端命令

- `ping`：发送心跳
- `quit`：退出

### 压力测试

```bash
# 发送 1000 条消息，验证 codec 粘包处理
./build/bin/stresstest 1000
```

## 通信协议

详见 [docs/protocol.md](docs/protocol.md)

消息格式：

```text
[4字节长度头][JSON payload]
```

PING 请求示例：

```json
{"msgid": 1, "data": "ping"}
```

PONG 响应示例：

```json
{"msgid": 2, "data": "pong"}
```

## 技术要点

1. **粘包/半包处理**

   TCP 是字节流协议，不保证消息边界，因此采用 `length-prefix framing`：

   ```text
   +-------------------+---------------------------+
   | length (4 bytes)  |       JSON payload        |
   +-------------------+---------------------------+
   ```

   解码步骤：

   - 可读字节 `< 4`：等待（半包）
   - 读出 `length` 后，若可读字节 `< 4 + length`：继续等待（半包）
   - 取出完整消息，调用业务回调
   - 继续循环，处理粘包

2. **线程模型**

   **Server：**
   - Main Loop（1 thread）：只做 `accept`
   - IO Loops（N threads）：处理读写事件

   **Client：**
   - IO Thread：`EventLoop`，处理网络事件
   - Main Thread：`getline` 读取用户输入

3. **回调机制**

   使用 `std::function` + lambda 注册回调：

   - `ConnectionCallback`：连接建立/断开
   - `MessageCallback`：收到数据（经过 codec 解码后是完整 JSON）

## 版本记录

- `v0.1`：基础框架：echo + JSON 协议 + codec + 压力测试

## License

MIT
