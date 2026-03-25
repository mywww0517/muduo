# 里程碑④⑤实现总结（面试版）

## 核心任务
实现完整的聊天功能：好友系统、一对一聊天、群组聊天、离线消息存储与推送。

---

## 一、技术实现细节

### 1. **好友系统**

**数据库设计：**
```sql
CREATE TABLE friend (
    userid INT NOT NULL,
    friendid INT NOT NULL,
    PRIMARY KEY(userid, friendid)
);
```

**设计要点：**
- 复合主键保证唯一性
- 双向关系：A 添加 B 需插入两条记录（A→B, B→A）
- 查询优化：JOIN user 表获取好友详细信息及在线状态

**实现代码：**
```cpp
// friendmodel.cpp
std::vector<User> FriendModel::query(int userid) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT u.id, u.name, u.state FROM user u "
        "INNER JOIN friend f ON f.friendid = u.id WHERE f.userid = %d", userid);

    // 执行查询，返回好友列表（包含在线状态）
}
```

**业务流程：**
1. 客户端发送 `ADD_FRIEND_MSG`（包含 userid 和 friendid）
2. 服务端插入 friend 表
3. 返回 `ADD_FRIEND_ACK`（errno=0 表示成功）
4. 登录时自动查询并返回好友列表

---

### 2. **一对一聊天（在线转发 / 离线存储）**

**核心逻辑：**
```cpp
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int toid = js["to"].get<int>();

    // 1. 查找目标用户是否在线
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end()) {
            // 在线：直接转发
            codecSend(it->second, js.dump());
            return;
        }
    }

    // 2. 离线：存储到数据库
    offlineMsgModel_.insert(toid, js.dump());
}
```

**技术亮点：**
1. **O(1) 在线查找**：通过 `userConnMap_`（unordered_map）快速判断在线状态
2. **消息完整性**：离线消息存储完整 JSON（包含 msgid、发送者、内容）
3. **线程安全**：访问 `userConnMap_` 时加锁保护
4. **自动推送**：登录时自动拉取离线消息并清空

**消息格式：**
```json
{
  "msgid": 20,
  "id": 1,        // 发送者
  "to": 2,        // 接收者
  "msg": "Hello"
}
```

---

### 3. **群组聊天（广播机制）**

**数据库设计：**
```sql
-- 群组表
CREATE TABLE allgroup (
    id INT PRIMARY KEY AUTO_INCREMENT,
    groupname VARCHAR(50) NOT NULL UNIQUE,
    groupdesc VARCHAR(200) DEFAULT ''
);

-- 群成员表
CREATE TABLE groupuser (
    groupid INT NOT NULL,
    userid INT NOT NULL,
    grouprole ENUM('creator', 'normal') DEFAULT 'normal',
    PRIMARY KEY(groupid, userid)
);
```

**广播逻辑：**
```cpp
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int groupid = js["groupid"].get<int>();
    int userid = js["id"].get<int>();

    // 1. 查询群成员列表
    std::vector<int> users = groupModel_.queryGroupUsers(groupid);

    // 2. 遍历成员，逐个转发
    std::lock_guard<std::mutex> lock(connMutex_);
    for (int id : users) {
        if (id == userid) continue;  // 跳过发送者

        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end()) {
            codecSend(it->second, js.dump());  // 在线转发
        } else {
            offlineMsgModel_.insert(id, js.dump());  // 离线存储
        }
    }
}
```

**技术亮点：**
1. **批量查询优化**：一次查询获取所有群成员
2. **混合转发**：在线成员实时转发，离线成员存储
3. **发送者过滤**：避免消息回显
4. **角色管理**：区分 creator 和 normal（为后续权限管理预留）

---

### 4. **离线消息系统**

**数据库设计：**
```sql
CREATE TABLE offlinemessage (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    userid INT NOT NULL,
    message TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX(userid)
);
```

**设计要点：**
- `message` 字段存储完整 JSON（保留消息类型、发送者等信息）
- `userid` 索引加速查询
- `created_at` 记录时间（可用于排序、过期清理）

**推送流程：**
```cpp
// 登录时
void ChatService::login(...) {
    // ... 验证密码、更新状态 ...

    // 查询离线消息
    std::vector<std::string> offlineMsgs = offlineMsgModel_.query(id);
    if (!offlineMsgs.empty()) {
        response["offlinemsgs"] = offlineMsgs;
        offlineMsgModel_.remove(id);  // 推送后立即清空
    }

    codecSend(conn, response.dump());
}
```

**技术亮点：**
1. **一次性推送**：登录响应包含所有离线消息
2. **推送即清空**：避免重复推送
3. **消息完整性**：客户端可根据 msgid 区分消息类型（一对一 / 群聊）

---

### 5. **登录数据加载（一次性返回）**

**设计思路：**
登录成功后，一次性返回用户所需的所有数据，减少后续请求。

**返回数据：**
```json
{
  "msgid": 6,
  "errno": 0,
  "id": 1,
  "name": "alice",
  "friends": [
    {"id": 2, "name": "bob", "state": "online"},
    {"id": 3, "name": "charlie", "state": "offline"}
  ],
  "groups": [
    {
      "id": 1,
      "name": "MyGroup",
      "desc": "Test group",
      "users": [
        {"id": 1, "name": "alice", "state": "online"},
        {"id": 2, "name": "bob", "state": "online"}
      ]
    }
  ],
  "offlinemsgs": [
    "{\"msgid\":20,\"id\":2,\"to\":1,\"msg\":\"Hello\"}",
    "{\"msgid\":34,\"groupid\":1,\"id\":3,\"msg\":\"Hi all\"}"
  ]
}
```

**实现代码：**
```cpp
// 1. 查询好友列表
std::vector<User> friends = friendModel_.query(id);
if (!friends.empty()) {
    json friendList = json::array();
    for (auto& f : friends) {
        json item;
        item["id"] = f.id();
        item["name"] = f.name();
        item["state"] = f.state();  // 显示在线状态
        friendList.push_back(item);
    }
    response["friends"] = friendList;
}

// 2. 查询群组列表（包含成员信息）
std::vector<GroupInfo> groups = groupModel_.queryGroups(id);
// ... 构造 JSON ...

// 3. 查询离线消息
std::vector<std::string> offlineMsgs = offlineMsgModel_.query(id);
if (!offlineMsgs.empty()) {
    response["offlinemsgs"] = offlineMsgs;
    offlineMsgModel_.remove(id);
}
```

**技术亮点：**
1. **减少往返**：一次请求获取所有数据
2. **实时状态**：好友列表包含当前在线状态
3. **完整信息**：群组列表包含所有成员详情

---

## 二、架构设计

### 分层结构

```
Application Layer (chatserver.cpp)
    ↓
Service Layer (chatservice.cpp)
    ↓ 消息路由
    ├─ UserModel      (用户认证)
    ├─ FriendModel    (好友管理)
    ├─ GroupModel     (群组管理)
    └─ OfflineMsgModel (离线消息)
    ↓
DB Layer (db.cpp)
    ↓
MySQL
```

### 消息流转

**一对一聊天（在线）：**
```
Client A → TcpServer → codecOnMessage → ChatService::oneChat
  → userConnMap_.find(B) → codecSend(connB) → Client B
```

**一对一聊天（离线）：**
```
Client A → TcpServer → codecOnMessage → ChatService::oneChat
  → userConnMap_.find(B) [not found]
  → OfflineMsgModel::insert → MySQL
```

**群聊消息：**
```
Client A → TcpServer → codecOnMessage → ChatService::groupChat
  → GroupModel::queryGroupUsers → [user1, user2, user3]
  → for each user:
      if online: codecSend
      else: OfflineMsgModel::insert
```

---

## 三、协议设计

### 消息类型扩展

```cpp
enum MsgId {
    // ... 已有消息类型 ...

    ADD_FRIEND_MSG  = 10,  // 添加好友请求
    ADD_FRIEND_ACK  = 11,  // 添加好友响应

    CHAT_MSG        = 20,  // 一对一聊天

    CREATE_GROUP_MSG = 30, // 创建群组请求
    CREATE_GROUP_ACK = 31, // 创建群组响应
    JOIN_GROUP_MSG   = 32, // 加入群组请求
    JOIN_GROUP_ACK   = 33, // 加入群组响应
    GROUP_CHAT_MSG   = 34, // 群聊消息
};
```

### 协议示例

**一对一聊天：**
```json
// 请求
{"msgid": 20, "id": 1, "to": 2, "msg": "Hello"}

// 无响应（直接转发或存储）
```

**创建群组：**
```json
// 请求
{"msgid": 30, "id": 1, "groupname": "MyGroup", "groupdesc": "Test"}

// 响应
{"msgid": 31, "errno": 0, "groupid": 1}
```

**群聊消息：**
```json
// 请求
{"msgid": 34, "id": 1, "groupid": 1, "msg": "Hello everyone"}

// 广播（原样转发给所有成员）
```

---

## 四、线程安全设计

### 1. **连接映射保护**
```cpp
std::unordered_map<int, TcpConnectionPtr> userConnMap_;
std::mutex connMutex_;

// 所有访问都加锁
{
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = userConnMap_.find(userid);
    // ...
}
```

### 2. **数据库访问保护**
```cpp
class FriendModel {
    MySQL mysql_;
    std::mutex dbMutex_;  // 每个 Model 独立锁
};
```

**原因：** muduo 是多线程 Reactor，多个 IO 线程可能同时调用业务层。

---

## 五、客户端实现

### 命令解析

```cpp
void handleChatMenu(const std::string &cmd) {
    if (cmd.substr(0, 4) == "chat") {
        // 解析：chat <friendid> <msg>
        std::istringstream iss(cmd);
        std::string c, msg;
        int friendid;
        iss >> c >> friendid;
        std::getline(iss, msg);

        json j;
        j["msgid"] = CHAT_MSG;
        j["id"] = currentUserId_;
        j["to"] = friendid;
        j["msg"] = msg.substr(1);  // 去掉前导空格
        send(j.dump());
    }
    // ... 其他命令 ...
}
```

### 消息接收

```cpp
void onJsonMessage(...) {
    json j = json::parse(message);
    int msgid = j["msgid"].get<int>();

    if (msgid == CHAT_MSG) {
        std::cout << "\n[好友消息] from=" << j["id"].get<int>()
                  << " msg: " << j["msg"].get<std::string>() << std::endl;
    }
    else if (msgid == GROUP_CHAT_MSG) {
        std::cout << "\n[群消息] groupid=" << j["groupid"].get<int>()
                  << " from=" << j["id"].get<int>()
                  << " msg: " << j["msg"].get<std::string>() << std::endl;
    }
}
```

---

## 六、测试验证

### 1. 好友系统测试

```bash
# 终端 1：用户 A 登录
./build/bin/chatclient
2 → 1 → 123456
addfriend 2
# 输出：✅ 添加好友成功

# 终端 2：用户 B 登录
./build/bin/chatclient
2 → 2 → 123456
# 登录响应包含好友列表（包含用户 A）
```

### 2. 一对一聊天测试

```bash
# 终端 1：用户 A 发送消息
chat 2 Hello, Bob!

# 终端 2：用户 B 立即收到
# [好友消息] from=1 msg: Hello, Bob!
```

### 3. 离线消息测试

```bash
# 终端 2：用户 B 退出
quit

# 终端 1：用户 A 发送消息
chat 2 Are you there?

# 终端 2：用户 B 重新登录
2 → 2 → 123456
# 登录响应包含离线消息
```

### 4. 群聊测试

```bash
# 终端 1：用户 A 创建群组
creategroup TestGroup
# 输出：✅ 创建群组成功，群ID=1

# 终端 2：用户 B 加入群组
joingroup 1
# 输出：✅ 加入群组成功

# 终端 1：用户 A 发送群消息
groupchat 1 Hello, everyone!

# 终端 2：用户 B 收到
# [群消息] groupid=1 from=1 msg: Hello, everyone!
```

---

## 七、面试重点

### 技术亮点

1. **消息路由优化**：O(1) 在线查找（unordered_map）
2. **混合转发机制**：在线实时转发 + 离线存储
3. **数据加载优化**：登录时一次性返回所有数据
4. **线程安全**：mutex 保护共享资源
5. **协议扩展性**：msgid 路由，易于添加新功能
6. **分层设计**：Service / Model / DB 职责清晰

### 可优化点（展示思考深度）

1. **SQL 注入防护**：当前用 snprintf 拼接，应改用预编译语句
2. **消息存储优化**：离线消息存储完整 JSON，占用空间大，可单独设计消息表
3. **好友申请机制**：当前直接添加，应改为申请-同意流程
4. **群组权限**：仅区分 creator 和 normal，可扩展管理员、禁言等功能
5. **消息顺序保证**：离线消息无序号，可能乱序，应添加全局递增 ID
6. **已读未读**：未实现消息已读状态
7. **消息撤回**：未实现撤回功能
8. **连接池**：每个 Model 独立连接，高并发时可用连接池

### 设计权衡

**为什么离线消息存储完整 JSON？**
- **优点**：实现简单，消息完整性好，客户端可直接解析
- **缺点**：存储空间大，查询效率低
- **改进**：单独设计消息表，offlinemessage 只存消息 ID

**为什么群聊消息逐个转发？**
- **优点**：实现简单，每个成员独立处理（在线/离线）
- **缺点**：性能较低，大群场景下遍历成员耗时
- **改进**：批量查询在线状态，批量发送

**为什么登录时一次性返回所有数据？**
- **优点**：减少往返次数，客户端体验好
- **缺点**：数据量大时响应慢
- **改进**：分页加载，或按需加载

---

## 八、关键代码位置

- 好友模型：[src/model/friendmodel.cpp](src/model/friendmodel.cpp)
- 群组模型：[src/model/groupmodel.cpp](src/model/groupmodel.cpp)
- 离线消息模型：[src/model/offlinemsgmodel.cpp](src/model/offlinemsgmodel.cpp)
- 一对一聊天：[src/server/chatservice.cpp:255-265](src/server/chatservice.cpp#L255-L265)
- 群聊广播：[src/server/chatservice.cpp:313-330](src/server/chatservice.cpp#L313-L330)
- 登录数据加载：[src/server/chatservice.cpp:105-165](src/server/chatservice.cpp#L105-L165)
- 协议定义：[src/common/protocol.hpp](src/common/protocol.hpp)
- 客户端命令处理：[src/client/chatclient.cpp:605-710](src/client/chatclient.cpp#L605-L710)

---

## 九、项目亮点总结

### 1. 完整的聊天功能
- 好友系统、一对一聊天、群组聊天、离线消息
- 覆盖即时通讯核心场景

### 2. 高性能设计
- O(1) 在线查找
- 混合转发机制（在线/离线）
- 批量查询优化

### 3. 良好的扩展性
- msgid 路由机制
- 分层架构
- 协议易扩展

### 4. 工程实践
- 线程安全设计
- 错误处理
- 日志记录
- 配置管理

### 5. 可演示性
- 完整的客户端交互
- 可视化的消息流转
- 易于复现的测试场景

---

## 十、面试话术示例

**面试官：介绍一下你的聊天系统项目。**

**回答：**
"这是一个基于 muduo 网络库和 MySQL 的即时通讯系统。我负责实现了完整的聊天功能，包括好友系统、一对一聊天、群组聊天和离线消息。

在技术实现上，我采用了混合转发机制：对于在线用户，通过内存中的连接映射（unordered_map）实现 O(1) 查找并实时转发；对于离线用户，将消息存储到 MySQL 的 offlinemessage 表，登录时自动推送。

在架构设计上，我采用了分层设计：Service 层负责业务逻辑和消息路由，Model 层负责数据访问，DB 层封装 MySQL 连接。这样职责清晰，易于维护和扩展。

在并发安全上，我使用 mutex 保护共享资源，因为 muduo 是多线程 Reactor，多个 IO 线程可能同时访问连接映射和数据库。

项目的亮点是实现了完整的聊天功能，并且性能较好，通过内存映射实现了快速的在线查找和消息转发。"

**面试官：如果离线消息很多，怎么优化？**

**回答：**
"当前实现是登录时一次性返回所有离线消息，如果消息量大确实会有问题。我会从以下几个方面优化：

1. **分页加载**：登录时只返回最近 N 条消息，客户端可以下拉加载更多
2. **消息表优化**：当前是存储完整 JSON，占用空间大。可以单独设计消息表，offlinemessage 只存消息 ID，减少存储和传输开销
3. **过期清理**：设置离线消息过期时间（如 7 天），定期清理过期消息
4. **消息压缩**：对于大量消息，可以在传输时进行压缩
5. **索引优化**：在 userid 和 created_at 上建立联合索引，加速分页查询

这些优化需要根据实际业务场景和数据量来选择，比如如果离线消息通常不多，当前实现就足够了。"

**面试官：群聊消息如何保证顺序？**

**回答：**
"当前实现没有保证消息顺序，因为群成员的在线/离线状态不同，处理时间也不同。要保证顺序，我会这样设计：

1. **消息序号**：为每条消息分配全局递增的序号（可以用 MySQL 的 AUTO_INCREMENT 或 Redis 的 INCR）
2. **客户端排序**：客户端根据序号排序显示，即使消息到达顺序不同，显示顺序也是正确的
3. **离线消息顺序**：数据库查询时按 created_at 或消息 ID 排序
4. **时间戳**：每条消息携带服务器时间戳，客户端可以根据时间戳排序

另外，如果要求更严格的顺序保证，可以考虑使用消息队列（如 Kafka）来保证消息的顺序性和可靠性。"

这样回答既展示了技术深度，又体现了工程实践能力和思考深度。
