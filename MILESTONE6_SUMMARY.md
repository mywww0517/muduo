# 里程碑⑥实现总结

## 完成内容

### 里程碑⑥：集群通信（Redis pub/sub）

#### 1. Redis 集成
- ✅ Redis 连接封装（`Redis` 类）
- ✅ 发布消息（`publish`）
- ✅ 订阅频道（`subscribe`）
- ✅ 取消订阅（`unsubscribe`）
- ✅ 消息回调处理（`observer_channel_message`）

#### 2. 用户频道订阅
- ✅ 登录时自动订阅个人频道（channel = userid）
- ✅ 登出时取消订阅
- ✅ 断线时取消订阅

#### 3. 跨服务器消息转发
- ✅ 一对一聊天支持跨服务器转发
- ✅ 群聊消息支持跨服务器转发
- ✅ 消息路由优化：本地在线 → 直接转发；跨服务器在线 → Redis 转发；离线 → 存储数据库

---

## 技术实现细节

### 1. Redis 封装类设计

#### Redis 类结构
```cpp
class Redis {
public:
    Redis();
    ~Redis();

    bool connect();
    bool publish(int channel, const std::string& message);
    bool subscribe(int channel);
    bool unsubscribe(int channel);
    void observer_channel_message(std::function<void(int, const std::string&)> fn);
    void init_notify_handler();

private:
    redisContext* publishContext_;      // 发布连接
    redisContext* subscribeContext_;    // 订阅连接
    std::function<void(int, const std::string&)> notifyMessageHandler_;
};
```

**设计要点：**
- **双连接设计**：发布和订阅使用独立连接，避免阻塞
- **发布连接**：用于发送消息到 Redis
- **订阅连接**：专门用于接收订阅消息，独立线程阻塞等待
- **回调机制**：通过 `observer_channel_message` 注册消息处理回调

#### 连接初始化
```cpp
bool Redis::connect() {
    publishContext_ = redisConnect("127.0.0.1", 6379);
    if (publishContext_ == nullptr || publishContext_->err) {
        return false;
    }

    subscribeContext_ = redisConnect("127.0.0.1", 6379);
    if (subscribeContext_ == nullptr || subscribeContext_->err) {
        return false;
    }

    // 启动独立线程处理订阅消息
    std::thread([this]() {
        init_notify_handler();
    }).detach();

    return true;
}
```

**关键点：**
- 创建两个独立的 Redis 连接
- 订阅连接在独立线程中运行，避免阻塞主线程
- 使用 detach() 让线程独立运行

#### 发布消息
```cpp
bool Redis::publish(int channel, const std::string& message) {
    redisReply* reply = (redisReply*)redisCommand(
        publishContext_,
        "PUBLISH %d %s",
        channel,
        message.c_str()
    );
    if (reply == nullptr) {
        return false;
    }
    freeReplyObject(reply);
    return true;
}
```

**关键点：**
- 使用 `PUBLISH` 命令发送消息到指定频道
- 频道号即为用户 ID
- 消息内容为完整的 JSON 字符串

#### 订阅频道
```cpp
bool Redis::subscribe(int channel) {
    if (redisAppendCommand(subscribeContext_, "SUBSCRIBE %d", channel) != REDIS_OK) {
        return false;
    }
    int done = 0;
    while (!done) {
        if (redisBufferWrite(subscribeContext_, &done) != REDIS_OK) {
            return false;
        }
    }
    return true;
}
```

**关键点：**
- 使用 `SUBSCRIBE` 命令订阅频道
- `redisAppendCommand` + `redisBufferWrite` 异步发送命令
- 订阅后，消息会在 `init_notify_handler` 线程中接收

#### 消息接收处理
```cpp
void Redis::init_notify_handler() {
    while (true) {
        redisReply* reply = (redisReply*)redisCommand(subscribeContext_, "");
        if (reply == nullptr) {
            break;
        }

        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
            if (std::string(reply->element[0]->str) == "message") {
                if (notifyMessageHandler_) {
                    notifyMessageHandler_(
                        std::atoi(reply->element[1]->str),  // channel (userid)
                        reply->element[2]->str              // message
                    );
                }
            }
        }
        freeReplyObject(reply);
    }
}
```

**关键点：**
- 独立线程阻塞等待 Redis 消息
- 解析 Redis 返回的数组：`["message", channel, content]`
- 调用回调函数处理消息

---

### 2. ChatService 集成 Redis

#### 初始化 Redis
```cpp
bool ChatService::init() {
    // ... 初始化其他模块 ...

    if (!redis_.connect()) {
        LOG_ERROR << "ChatService init failed: redis connect error";
        return false;
    }

    // 注册 Redis 消息回调
    redis_.observer_channel_message(
        std::bind(&ChatService::handleRedisSubscribeMessage,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2)
    );

    return true;
}
```

**关键点：**
- 服务启动时连接 Redis
- 注册消息回调函数 `handleRedisSubscribeMessage`
- 回调函数在 Redis 线程中执行

#### 登录时订阅频道
```cpp
void ChatService::login(...) {
    // ... 验证密码、更新状态 ...

    // 订阅 Redis 频道
    redis_.subscribe(id);

    // ... 返回好友列表、群组列表、离线消息 ...
}
```

**关键点：**
- 用户登录成功后立即订阅个人频道
- 频道号 = 用户 ID
- 订阅后可以接收其他服务器转发的消息

#### 登出时取消订阅
```cpp
void ChatService::logout(...) {
    // ... 清理连接映射 ...

    redis_.unsubscribe(userid);

    // ... 更新数据库状态 ...
}
```

#### 断线时取消订阅
```cpp
void ChatService::clientCloseException(const TcpConnectionPtr& conn) {
    // ... 查找用户 ID ...

    if (user.id() != -1) {
        redis_.unsubscribe(user.id());
        // ... 更新状态 ...
    }
}
```

---

### 3. 跨服务器消息转发

#### 一对一聊天转发逻辑
```cpp
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int toid = js["to"].get<int>();

    // 1. 查找本地连接
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end()) {
            codecSend(it->second, js.dump());  // 本地在线，直接转发
            return;
        }
    }

    // 2. 查询数据库状态
    User user = userModel_.query(toid);
    if (user.id() != -1 && user.state() == "online") {
        // 用户在线但不在本地 → 跨服务器转发
        redis_.publish(toid, js.dump());
    } else {
        // 用户离线 → 存储到数据库
        offlineMsgModel_.insert(toid, js.dump());
    }
}
```

**消息路由策略：**
1. **本地在线**：直接通过 TCP 连接转发（最快）
2. **跨服务器在线**：通过 Redis 发布到目标用户频道
3. **离线**：存储到数据库，登录时推送

**关键点：**
- 先查本地连接（O(1)）
- 再查数据库状态（判断是否在其他服务器在线）
- 根据状态选择转发方式

#### 群聊消息转发逻辑
```cpp
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int groupid = js["groupid"].get<int>();
    int userid = js["id"].get<int>();

    std::vector<int> users = groupModel_.queryGroupUsers(groupid);

    std::lock_guard<std::mutex> lock(connMutex_);
    for (int id : users) {
        if (id == userid) continue;  // 跳过发送者

        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end()) {
            codecSend(it->second, js.dump());  // 本地在线
        } else {
            User user = userModel_.query(id);
            if (user.id() != -1 && user.state() == "online") {
                redis_.publish(id, js.dump());  // 跨服务器在线
            } else {
                offlineMsgModel_.insert(id, js.dump());  // 离线
            }
        }
    }
}
```

**关键点：**
- 遍历群成员，逐个判断在线状态
- 每个成员独立选择转发方式
- 支持群成员分布在不同服务器

#### Redis 消息回调处理
```cpp
void ChatService::handleRedisSubscribeMessage(int userid, const std::string& msg) {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = userConnMap_.find(userid);
    if (it != userConnMap_.end()) {
        codecSend(it->second, msg);
    }
}
```

**关键点：**
- 在 Redis 线程中回调
- 查找本地连接并转发消息
- 如果用户已经下线，消息会丢失（因为已经取消订阅）

---

## 架构设计

### 集群架构

```
┌─────────────┐         ┌─────────────┐
│  Client A   │         │  Client B   │
└──────┬──────┘         └──────┬──────┘
       │                       │
       │ TCP                   │ TCP
       │                       │
┌──────▼──────┐         ┌──────▼──────┐
│ ChatServer1 │         │ ChatServer2 │
│  (Port 8888)│         │  (Port 8889)│
└──────┬──────┘         └──────┬──────┘
       │                       │
       │ Redis pub/sub         │
       └───────┬───────────────┘
               │
        ┌──────▼──────┐
        │    Redis    │
        │  (Port 6379)│
        └──────┬──────┘
               │
        ┌──────▼──────┐
        │    MySQL    │
        │  (Port 3306)│
        └─────────────┘
```

### 消息流转

**场景 1：本地在线用户**
```
Client A (Server1) → Server1 → 本地转发 → Client B (Server1)
```

**场景 2：跨服务器在线用户**
```
Client A (Server1) → Server1 → Redis publish → Server2 → Client B (Server2)
```

**场景 3：离线用户**
```
Client A (Server1) → Server1 → MySQL → (等待用户登录) → Server2 → Client B
```

---

## 编译配置

**CMakeLists.txt 关键修改：**
```cmake
# 添加 Redis 头文件路径
include_directories(${CMAKE_SOURCE_DIR}/src/redis)

# 添加 Redis 源文件
add_executable(chatserver
    src/server/chatserver.cpp
    src/server/chatservice.cpp
    src/db/db.cpp
    src/model/usermodel.cpp
    src/model/friendmodel.cpp
    src/model/groupmodel.cpp
    src/model/offlinemsgmodel.cpp
    src/redis/redis.cpp          # 新增
    src/common/crypto.cpp
)

# 链接 hiredis 库
target_link_libraries(chatserver ${MUDUO_LIBS} mysqlclient crypto hiredis)
```

---

## 测试验证

### 1. 启动 Redis
```bash
redis-server
```

### 2. 启动多个服务器实例
```bash
# 终端 1：启动服务器 1（端口 8888）
export CHAT_DB_USER=vscode CHAT_DB_PASSWORD='050517Wzx' CHAT_DB_NAME=chat
./build/bin/chatserver

# 终端 2：启动服务器 2（端口 8889）
export CHAT_DB_USER=vscode CHAT_DB_PASSWORD='050517Wzx' CHAT_DB_NAME=chat
./build/bin/chatserver 8889
```

### 3. 测试跨服务器通信
```bash
# 终端 3：客户端 A 连接服务器 1
./build/bin/chatclient
2 → 1 → 123456

# 终端 4：客户端 B 连接服务器 2
./build/bin/chatclient 127.0.0.1 8889
2 → 2 → 123456

# 终端 3：客户端 A 发送消息给客户端 B
chat 2 Hello from Server1!

# 终端 4：客户端 B 收到消息
# [好友消息] from=1 msg: Hello from Server1!
```

---

## 关键代码位置

- Redis 封装：[src/redis/redis.cpp](src/redis/redis.cpp)
- Redis 头文件：[src/redis/redis.hpp](src/redis/redis.hpp)
- Redis 集成：[src/server/chatservice.cpp:26-48](src/server/chatservice.cpp#L26-L48)
- 登录订阅：[src/server/chatservice.cpp:147](src/server/chatservice.cpp#L147)
- 登出取消订阅：[src/server/chatservice.cpp:211](src/server/chatservice.cpp#L211)
- 一对一转发：[src/server/chatservice.cpp:276-291](src/server/chatservice.cpp#L276-L291)
- 群聊转发：[src/server/chatservice.cpp:339-358](src/server/chatservice.cpp#L339-L358)
- Redis 回调：[src/server/chatservice.cpp:253-259](src/server/chatservice.cpp#L253-L259)

---

## 已知限制与改进方向

### 当前限制

1. **Redis 单点故障**：Redis 挂掉后集群通信失败
2. **消息可靠性**：Redis pub/sub 不保证消息可靠性，订阅者离线时消息丢失
3. **连接管理**：Redis 连接断开后未自动重连
4. **配置硬编码**：Redis 地址端口硬编码在代码中
5. **无负载均衡**：客户端需要手动指定服务器地址

### 改进方向

1. **Redis 高可用**：使用 Redis Sentinel 或 Redis Cluster
2. **消息队列**：改用 Redis Stream 或 RabbitMQ 保证消息可靠性
3. **自动重连**：实现 Redis 连接断开自动重连机制
4. **配置文件化**：Redis 配置从文件读取
5. **负载均衡**：使用 Nginx / LVS 实现负载均衡
6. **服务注册**：使用 etcd / Consul 实现服务注册与发现
7. **心跳检测**：定期检测 Redis 连接状态

---

## 交付清单

- [x] Redis 连接封装（双连接设计）
- [x] 发布消息（publish）
- [x] 订阅频道（subscribe）
- [x] 取消订阅（unsubscribe）
- [x] 消息回调处理（observer_channel_message）
- [x] 登录时订阅个人频道
- [x] 登出/断线时取消订阅
- [x] 一对一聊天跨服务器转发
- [x] 群聊消息跨服务器转发
- [x] 消息路由优化（本地 → Redis → 离线）
- [x] 编译通过（链接 hiredis）
- [x] README 更新到 v0.5
- [x] 验收文档（本文档）
