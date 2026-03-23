# MyMuduo Chat v0.2

一个基于 **C++11 / muduo / MySQL** 的轻量级聊天服务器学习项目。  
项目重点练习以下内容：

- Linux 下的网络编程与 Reactor 模型
- 自定义应用层协议与消息编解码
- TCP 粘包 / 半包处理
- 服务端业务分层与消息路由
- 用户注册 / 登录 / 登出与数据库持久化

> 当前版本 **v0.2** 的重点是打通“认证链路”：
>
> **Client -> codec -> JSON -> ChatService -> UserModel -> MySQL**

---

## 项目简介

`MyMuduo Chat` 从 v0.1 的基础 echo / ping-pong + codec 出发，逐步演进为一个具备用户系统雏形的聊天服务端。

相比 v0.1，v0.2 主要新增了：

- 用户注册 / 登录 / 登出流程
- 基于 **MySQL** 的用户数据持久化
- `ChatService` 业务层，引入按 `msgid` 路由分发的设计
- `UserModel` 数据访问层，负责用户表操作
- `DB` 数据库封装层，统一管理 MySQL 连接
- 客户端从“自由输入文本”升级为“菜单式交互”
- 服务端通过环境变量读取数据库配置
- 项目结构从网络样例演进为分层聊天系统

---

## 项目特点

- 基于 **muduo** 的 Reactor 模型（epoll + 非阻塞 IO）
- 使用 **JSON** 作为应用层消息格式（`nlohmann/json`）
- 自定义 **length-prefix codec**，解决 TCP 粘包 / 半包问题
- 基于 `msgid` 的消息路由分发机制
- 引入 `ChatService / UserModel / DB` 的基础分层设计
- 基于 **MySQL** 持久化用户信息并维护在线状态
- 客户端支持菜单式交互与基本状态管理
- 提供脚本、协议文档与架构文档，便于构建和复现

---

## 当前功能

### 服务端

- 接收客户端 JSON 请求
- 按 `msgid` 分发到对应业务处理函数
- 当前支持：
  - `PING_MSG`
  - `REG_MSG`
  - `LOGIN_MSG`
  - `LOGOUT_MSG`
- 启动时初始化数据库连接
- 数据库初始化失败时直接退出，避免带着错误状态运行

### 客户端

- 连接服务端
- 未登录状态支持：
  - `register`（注册）
  - `login`（登录）
  - `quit`（退出）
- 已登录状态支持：
  - `ping`（心跳测试）
  - `logout`（登出）
  - `quit`（退出程序）

---

## 当前版本说明

- v0.2 当前重点是完成 **注册 / 登录认证链路闭环**
- 在线状态目前以数据库字段维护为主，后续仍可继续优化
- 当前版本以功能验证和架构演进为主，**密码存储采用简化方案**
- 生产环境中应使用 **加盐哈希** 等更安全的密码存储方式
- 登出流程当前已支持请求发送与客户端状态切换，后续可进一步完善为严格 ACK 确认机制

---

## 快速开始

### 1. 环境要求

- Linux（推荐 Ubuntu 18.04+）
- GCC 7+（支持 C++11）
- CMake 3.10+
- muduo 网络库
- MySQL 5.7+ / 8.0+

---

### 2. 安装依赖

```bash
sudo apt-get update
sudo apt-get install -y g++ cmake make libboost-all-dev default-libmysqlclient-dev
```

> `nlohmann/json` 以单头文件形式放在 `thirdparty/json.hpp` 中，无需额外安装。

---

### 3. 安装 muduo

```bash
git clone https://github.com/chenshuo/muduo.git
cd muduo
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build -j$(nproc)
sudo cmake --install build
```

---

### 4. 准备 MySQL

#### 4.1 创建数据库

```sql
CREATE DATABASE IF NOT EXISTS chat DEFAULT CHARACTER SET utf8mb4;
```

#### 4.2 创建用户表

```sql
USE chat;

CREATE TABLE IF NOT EXISTS user (
    id INT PRIMARY KEY AUTO_INCREMENT,
    name VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(50) NOT NULL,
    state ENUM('online', 'offline') NOT NULL DEFAULT 'offline'
);
```

#### 4.3 准备 MySQL 账号

你可以直接使用已有账号，也可以新建一个本地开发用账号，例如：

```sql
CREATE USER 'vscode'@'127.0.0.1' IDENTIFIED BY 'your_password';
GRANT ALL PRIVILEGES ON chat.* TO 'vscode'@'127.0.0.1';
FLUSH PRIVILEGES;
```

> 以上授权方式仅用于本地开发测试。  
> 建议使用 `127.0.0.1`，不要混用 `localhost`，便于与程序中的配置保持一致。

---

### 5. 配置数据库环境变量

服务端通过环境变量读取数据库配置。  
建议在项目中使用：

- `config/db.env`：本地实际配置，**不要提交敏感信息**
- `config/db.env.example`：配置模板，建议提交到仓库

#### `config/db.env.example`

```bash
CHAT_DB_HOST=127.0.0.1
CHAT_DB_PORT=3306
CHAT_DB_USER=your_user
CHAT_DB_PASSWORD=your_password
CHAT_DB_NAME=chat
```

#### `config/db.env`

```bash
CHAT_DB_HOST=127.0.0.1
CHAT_DB_PORT=3306
CHAT_DB_USER=vscode
CHAT_DB_PASSWORD=your_password
CHAT_DB_NAME=chat
```


---

### 6. 克隆并编译项目

```bash
git clone https://github.com/LaAutre/mymuduo.git
cd mymuduo
./scripts/build.sh
```

或手动编译：

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

---

### 7. 运行项目

#### 方式一：使用脚本启动（推荐）

```bash
./scripts/run_server.sh
./scripts/run_client.sh
```

建议在 `run_server.sh` 中自动加载数据库环境变量，例如：

```bash
#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

set -a
source "$PROJECT_ROOT/config/db.env"
set +a

exec "$PROJECT_ROOT/build/bin/chatserver"
```

#### 方式二：手动启动

终端 1：启动服务端

```bash
cd ~/mymuduo
set -a
source config/db.env
set +a
./build/bin/chatserver
```

终端 2：启动客户端

```bash
cd ~/mymuduo
./build/bin/chatclient
```

> 注意：  
> `source config/db.env` 和 `./build/bin/chatserver` 必须在同一个 shell 中执行，否则环境变量不会传递给服务端进程。

---

## 客户端使用方式

### 未登录状态菜单

```text
1. register  (注册)
2. login     (登录)
3. quit      (退出)
```

### 已登录状态菜单

```text
ping    - 心跳测试
logout  - 登出
quit    - 退出程序
```

---

## 示例运行流程

### 1）注册

客户端选择：

```text
1
```

输入用户名和密码，例如：

```text
用户名: alice
密码: 123456
```

若成功，客户端输出类似：

```text
✅ 注册成功！你的 ID = 1
```

---

### 2）登录

客户端选择：

```text
2
```

输入用户 ID 和密码：

```text
用户 ID: 1
密码: 123456
```

若成功，客户端输出类似：

```text
✅ 登录成功！欢迎 alice (id=1)
```

---

### 3）心跳测试

登录后输入：

```text
ping
```

返回类似：

```text
[PONG] pong
```

---

### 4）登出

登录后输入：

```text
logout
```

客户端回到未登录状态。

---

## 架构设计

详细说明可参考：

- [docs/protocol.md](docs/protocol.md)
- [docs/architecture.md](docs/architecture.md)

### 总体分层

```text
┌──────────────────────────────────────────────────────┐
│                  Application Layer                   │
│             ChatServer / ChatClient                  │
├──────────────────────────────────────────────────────┤
│                    Service Layer                     │
│      ChatService (业务分发 / handler 映射)           │
├──────────────────────────────────────────────────────┤
│                     Model Layer                      │
│        UserModel (用户数据访问) / DB (MySQL)         │
├──────────────────────────────────────────────────────┤
│                   Protocol Layer                     │
│   codec (length-prefix framing) + JSON 序列化        │
├──────────────────────────────────────────────────────┤
│                    Network Layer                     │
│       muduo (TcpServer / TcpClient / EventLoop)      │
├──────────────────────────────────────────────────────┤
│                    System Layer                      │
│         Linux (epoll / socket / pthread)             │
└──────────────────────────────────────────────────────┘
```

### 服务端调用链

```text
TcpServer
   ↓
codecOnMessage
   ↓
json::parse
   ↓
根据 msgid 获取 handler
   ↓
ChatService
   ↓
UserModel
   ↓
DB / MySQL
```

---

## 目录结构

```text
mymuduo/
├── CMakeLists.txt
├── README.md
├── config/
│   ├── db.env                      # 本地数据库环境变量（不提交敏感信息）
│   └── db.env.example              # 环境变量模板
├── docs/
│   ├── architecture.md             # 架构说明
│   ├── database.md                 # 数据库设计说明
│   └── protocol.md                 # 通信协议说明
├── include/                        # 预留公共头文件目录
├── scripts/
│   ├── build.sh                    # 一键编译
│   ├── run_client.sh               # 启动客户端
│   ├── run_server.sh               # 启动服务端
│   └── build                       # 其他本地脚本/文件（如无必要建议清理或重命名）
├── src/
│   ├── client/
│   │   ├── chatclient.cpp          # 聊天客户端（菜单交互）
│   │   ├── echoclient.cpp          # 回声客户端
│   │   └── stresstest.cpp          # codec 粘包压力测试
│   ├── common/
│   │   ├── codec.hpp               # 编解码器（长度头分帧）
│   │   ├── json_helper.hpp         # JSON 辅助封装
│   │   └── protocol.hpp            # 消息 ID / 协议定义
│   ├── db/
│   │   ├── db.hpp                  # MySQL 封装
│   │   ├── db.cpp                  # 数据库连接实现
│   │   ├── test_db.cpp             # DB 层测试
│   │   └── test_model.cpp          # Model 层测试入口
│   ├── model/
│   │   ├── user.hpp                # 用户实体
│   │   ├── usermodel.hpp           # 用户数据模型
│   │   └── usermodel.cpp           # 用户表操作
│   └── server/
│       ├── chatserver.cpp          # 聊天服务端入口
│       ├── chatservice.hpp         # 业务层声明
│       ├── chatservice.cpp         # 业务层实现
│       └── echoserver.cpp          # 回声服务端
├── thirdparty/
│   └── json.hpp                    # nlohmann/json 单头文件
└── build/                          # 编译产物（不提交）
```

---

## 通信协议

详见：[docs/protocol.md](docs/protocol.md)

### 消息分帧格式

项目采用长度头协议：

```text
[4字节长度头][JSON payload]
```

### 设计说明

- TCP 是字节流协议，不保证消息边界
- 因此需要在应用层定义消息边界
- 本项目使用 **4 字节长度头 + JSON payload** 的方式进行分帧
- 解码器会在 `Buffer` 中循环解析完整包，从而同时处理：
  - 半包
  - 粘包

### 消息类型示意

以下 `msgid` 数值仅作示意，实际以 `src/common/protocol.hpp` 为准。

| 消息类型 | 说明 |
|---|---|
| `PING_MSG` | 心跳请求 |
| `PONG_MSG` | 心跳响应 |
| `REG_MSG` | 注册请求 |
| `REG_MSG_ACK` | 注册响应 |
| `LOGIN_MSG` | 登录请求 |
| `LOGIN_MSG_ACK` | 登录响应 |
| `LOGOUT_MSG` | 登出请求 |
| `ERROR_MSG` | 错误响应 |

### JSON 示例

#### 1）PING 请求

```json
{"msgid": 1, "data": "ping"}
```

#### 2）PONG 响应

```json
{"msgid": 2, "data": "pong"}
```

#### 3）注册请求

```json
{"msgid": 3, "name": "alice", "password": "123456"}
```

#### 4）注册响应

```json
{"msgid": 4, "errno": 0, "id": 1}
```

失败时可能类似：

```json
{"msgid": 4, "errno": 1, "errmsg": "username already exists"}
```

#### 5）登录请求

```json
{"msgid": 5, "id": 1, "password": "123456"}
```

#### 6）登录响应

```json
{"msgid": 6, "errno": 0, "id": 1, "name": "alice"}
```

失败时可能类似：

```json
{"msgid": 6, "errno": 1, "errmsg": "id or password error"}
```

#### 7）登出请求

```json
{"msgid": 7, "id": 1}
```

#### 8）错误响应

```json
{"msgid": 999, "error": "unknown msgid"}
```

或：

```json
{"msgid": 999, "errmsg": "invalid request"}
```

---

## 技术要点

### 1. TCP 粘包 / 半包处理

TCP 是字节流协议，一次 `send` 不一定对应一次 `recv`。  
为了解决消息边界问题，项目采用 **length-prefix framing**：

```text
+-------------------+---------------------------+
| length (4 bytes)  |       JSON payload        |
+-------------------+---------------------------+
```

解码逻辑如下：

1. 若可读字节 `< 4`，说明长度头还不完整，继续等待
2. 先读取 4 字节长度头
3. 若可读字节 `< 4 + length`，说明 payload 还没收全，继续等待
4. 取出完整消息并交给业务回调
5. 继续循环处理缓冲区后续数据，解决粘包问题

---

### 2. 消息路由与业务分层

v0.2 引入了比较明确的职责划分：

#### `chatserver.cpp`

负责：

- 启动 `TcpServer`
- 注册 muduo 回调
- 通过 codec 解包
- JSON 解析
- 将请求交给 `ChatService`

#### `chatservice.cpp`

负责：

- 注册消息处理函数
- 实现注册 / 登录 / 登出 / 心跳等业务
- 作为服务端业务调度中心

#### `usermodel.cpp`

负责：

- 用户插入
- 用户查询
- 用户状态更新（`online / offline`）

#### `db.cpp`

负责：

- 读取环境变量
- 建立 MySQL 连接
- 向上层提供数据库访问能力

---

### 3. 客户端状态管理

v0.2 的客户端不再只是“发字符串”，而是维护了基本状态，例如：

- 是否已连接
- 是否已登录
- 当前登录用户 ID

因此客户端交互从 v0.1 的：

```text
ping / quit
```

升级为：

```text
register / login / ping / logout / quit
```

这也是从“测试客户端”走向“业务客户端”的关键一步。

---

### 4. 回调机制

项目基于 muduo 的回调机制处理网络事件，常见回调包括：

- `ConnectionCallback`：连接建立 / 断开
- `MessageCallback`：收到网络数据
- `codecOnMessage(...)`：从 `Buffer` 中拆出完整消息
- `onJsonMessage(...)`：业务层处理 JSON 请求 / 响应

---

## 压力测试

项目保留了用于验证 codec 分帧逻辑的简单压力测试程序：

```bash
./build/bin/stresstest 1000
```

功能包括：

- 连续发送大量消息
- 检验服务端是否正确处理粘包 / 半包
- 验证 codec 在高频消息下是否稳定

---

## 常见问题

### 1）启动服务端时报错：`CHAT_DB_USER is not set`

说明当前 shell 没有加载数据库环境变量。

解决方式：

```bash
cd ~/mymuduo
set -a
source config/db.env
set +a
./build/bin/chatserver
```

检查是否加载成功：

```bash
env | grep CHAT_DB
```

---

### 2）报错：`Access denied for user ...`

说明 MySQL 用户名 / 密码 / host 不匹配。

建议检查：

- `CHAT_DB_HOST=127.0.0.1`
- `CHAT_DB_USER`
- `CHAT_DB_PASSWORD`
- MySQL 中是否存在对应的 `user@host`

---

### 3）报错：`Unknown database 'chat'`

说明数据库还没有创建。

请先执行：

```sql
CREATE DATABASE chat;
```

---

### 4）客户端显示已连接，但注册 / 登录无响应

请检查：

- 服务端是否正常启动
- codec 是否两端一致
- `protocol.hpp` 中 `msgid` 是否对应
- 客户端是否发送了正确 JSON 字段
- 服务端日志中是否有 JSON 解析错误或未知消息类型

---

### 5）为什么客户端注册 / 登录后有短暂 `sleep_for`？

这是 v0.2 中的一个简化处理，用于避免：

- 主线程菜单提示输出
- IO 回包输出

两者在终端中互相打架。

后续版本可以进一步改造成更严谨的 ACK / 条件同步机制。

---

## 开发路线

### 已完成

- [x] muduo 基础服务端 / 客户端
- [x] codec 分帧，解决 TCP 粘包 / 半包
- [x] JSON 协议收发
- [x] ping / pong
- [x] MySQL 接入
- [x] 用户注册
- [x] 用户登录
- [x] 用户登出
- [x] 客户端菜单化
- [x] 服务端基础分层（Service / Model / DB）

### 计划中

- [ ] 登录后的正式聊天命令
- [ ] 一对一聊天
- [ ] 群聊
- [ ] 离线消息
- [ ] 好友系统
- [ ] 更完整的错误码与 ACK 机制
- [ ] 更严格的状态一致性处理
- [ ] 配置文件化与脚本完善

---

## 版本记录

| 版本 | 说明 |
|---|---|
| v0.1 | 基础框架：echo + JSON 协议 + codec + 压力测试 |
| v0.2 | 用户系统：MySQL + 注册 / 登录 / 登出 + ChatService 分层 + 菜单客户端 |

---

## License

MIT
