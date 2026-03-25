# 里程碑④⑤实现总结

## 完成内容

### 里程碑④：单机聊天（好友聊天、群聊）

#### 1. 好友系统
- ✅ 添加好友（`ADD_FRIEND_MSG`）
- ✅ 查询好友列表（登录时返回）
- ✅ 好友在线状态显示
- ✅ 数据模型：`FriendModel`、`friend` 表

#### 2. 一对一聊天
- ✅ 发送消息给好友（`CHAT_MSG`）
- ✅ 在线用户：实时转发到目标连接
- ✅ 离线用户：存储到 `offlinemessage` 表
- ✅ 消息路由：通过 `userConnMap_` 查找在线状态

#### 3. 群组功能
- ✅ 创建群组（`CREATE_GROUP_MSG`）
- ✅ 加入群组（`JOIN_GROUP_MSG`）
- ✅ 群消息广播（`GROUP_CHAT_MSG`）
- ✅ 登录时返回群组列表及成员信息
- ✅ 数据模型：`GroupModel`、`allgroup` 表、`groupuser` 表

### 里程碑⑤：离线消息

#### 1. 离线消息存储
- ✅ 一对一聊天离线存储
- ✅ 群聊消息离线存储
- ✅ 数据模型：`OfflineMsgModel`、`offlinemessage` 表

#### 2. 离线消息推送
- ✅ 登录时自动拉取离线消息
- ✅ 推送后自动清空
- ✅ 消息格式：完整 JSON（包含 msgid、发送者、内容）

---

## 技术实现细节

### 1. 数据库表设计

#### friend 表（好友关系）
```sql
CREATE TABLE friend (
    userid INT NOT NULL,
    friendid INT NOT NULL,
    PRIMARY KEY(userid, friendid)
);
```

**设计要点：**
- 复合主键：(userid, friendid)
- 单向关系：A 添加 B 为好友，需要插入两条记录（A→B 和 B→A）
- 查询好友列表：JOIN user 表获取好友详细信息

#### allgroup 表（群组）
```sql
CREATE TABLE allgroup (
    id INT PRIMARY KEY AUTO_INCREMENT,
    groupname VARCHAR(50) NOT NULL UNIQUE,
    groupdesc VARCHAR(200) DEFAULT ''
);
```

#### groupuser 表（群成员）
```sql
CREATE TABLE groupuser (
    groupid INT NOT NULL,
    userid INT NOT NULL,
    grouprole ENUM('creator', 'normal') DEFAULT 'normal',
    PRIMARY KEY(groupid, userid)
);
```

**设计要点：**
- 复合主键：(groupid, userid)
- grouprole：区分群主和普通成员
- 查询群成员：JOIN user 表获取成员详细信息

#### offlinemessage 表（离线消息）
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
- message 字段存储完整 JSON 字符串
- userid 索引：加速查询
- created_at：记录消息时间（可用于排序）

---

### 2. 核心业务逻辑

#### 一对一聊天流程

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

**关键点：**
- 先查内存 `userConnMap_`（O(1) 查找）
- 在线直接转发，离线存储
- 消息格式保持完整 JSON（包含发送者信息）

#### 群聊消息广播

```cpp
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int groupid = js["groupid"].get<int>();
    int userid = js["id"].get<int>();

    // 1. 查询群成员列表
    std::vector<int> users = groupModel_.queryGroupUsers(groupid);

    // 2. 遍历成员，逐个转发
    std::lock_guard<std::mutex> lock(connMutex_);
    for (int id : users) {
        if (id == userid) continue;  // 跳过发送者自己

        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end()) {
            // 在线：转发
            codecSend(it->second, js.dump());
        } else {
            // 离线：存储
            offlineMsgModel_.insert(id, js.dump());
        }
    }
}
```

**关键点：**
- 先查询群成员列表（一次数据库查询）
- 遍历成员，逐个判断在线状态
- 跳过发送者自己（避免回显）

#### 登录时数据加载

```cpp
void ChatService::login(...) {
    // ... 验证密码、检查重复登录 ...

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

    // 2. 查询群组列表
    std::vector<GroupInfo> groups = groupModel_.queryGroups(id);
    if (!groups.empty()) {
        json groupList = json::array();
        for (auto& g : groups) {
            json item;
            item["id"] = g.group.id();
            item["name"] = g.group.name();
            item["desc"] = g.group.desc();
            json users = json::array();
            for (auto& u : g.users) {
                json uitem;
                uitem["id"] = u.id();
                uitem["name"] = u.name();
                uitem["state"] = u.state();
                users.push_back(uitem);
            }
            item["users"] = users;
            groupList.push_back(item);
        }
        response["groups"] = groupList;
    }

    // 3. 查询离线消息
    std::vector<std::string> offlineMsgs = offlineMsgModel_.query(id);
    if (!offlineMsgs.empty()) {
        response["offlinemsgs"] = offlineMsgs;
        offlineMsgModel_.remove(id);  // 推送后清空
    }

    codecSend(conn, response.dump());
}
```

**关键点：**
- 登录响应一次性返回所有数据
- 好友列表包含在线状态
- 群组列表包含成员详情
- 离线消息推送后立即清空

---

### 3. 协议设计

#### 添加好友

**请求：**
```json
{
  "msgid": 10,
  "id": 1,
  "friendid": 2
}
```

**响应：**
```json
{
  "msgid": 11,
  "errno": 0
}
```

#### 一对一聊天

**请求：**
```json
{
  "msgid": 20,
  "id": 1,
  "to": 2,
  "msg": "Hello, Bob!"
}
```

**转发/离线存储：**（原样存储完整 JSON）

#### 创建群组

**请求：**
```json
{
  "msgid": 30,
  "id": 1,
  "groupname": "MyGroup",
  "groupdesc": "This is my group"
}
```

**响应：**
```json
{
  "msgid": 31,
  "errno": 0,
  "groupid": 1
}
```

#### 加入群组

**请求：**
```json
{
  "msgid": 32,
  "id": 1,
  "groupid": 1
}
```

**响应：**
```json
{
  "msgid": 33,
  "errno": 0
}
```

#### 群聊消息

**请求：**
```json
{
  "msgid": 34,
  "id": 1,
  "groupid": 1,
  "msg": "Hello, everyone!"
}
```

**广播：**（原样转发给所有成员）

---

### 4. 客户端实现

#### 命令解析

```cpp
void handleChatMenu(const std::string &cmd) {
    if (cmd.substr(0, 4) == "chat") {
        std::istringstream iss(cmd);
        std::string c, msg;
        int friendid;
        iss >> c >> friendid;
        std::getline(iss, msg);
        // 发送 CHAT_MSG
    }
    else if (cmd.substr(0, 9) == "addfriend") {
        // 发送 ADD_FRIEND_MSG
    }
    else if (cmd.substr(0, 11) == "creategroup") {
        // 发送 CREATE_GROUP_MSG
    }
    else if (cmd.substr(0, 9) == "joingroup") {
        // 发送 JOIN_GROUP_MSG
    }
    else if (cmd.substr(0, 9) == "groupchat") {
        // 发送 GROUP_CHAT_MSG
    }
}
```

#### 消息接收处理

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

## 架构设计

### 分层结构

```
┌─────────────────────────────────────────────────────┐
│                  Application Layer                  │
│             ChatServer / ChatClient                 │
├─────────────────────────────────────────────────────┤
│                    Service Layer                    │
│      ChatService (业务分发 / handler 映射)          │
├─────────────────────────────────────────────────────┤
│                     Model Layer                     │
│  UserModel / FriendModel / GroupModel /             │
│  OfflineMsgModel (数据访问)                         │
├─────────────────────────────────────────────────────┤
│                   Database Layer                    │
│        DB (MySQL 连接封装)                          │
├─────────────────────────────────────────────────────┤
│                   Protocol Layer                    │
│   codec (length-prefix framing) + JSON 序列化       │
├─────────────────────────────────────────────────────┤
│                    Network Layer                    │
│       muduo (TcpServer / TcpClient / EventLoop)     │
└─────────────────────────────────────────────────────┘
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
  → OfflineMsgModel::insert → MySQL offlinemessage
```

**群聊消息：**
```
Client A → TcpServer → codecOnMessage → ChatService::groupChat
  → GroupModel::queryGroupUsers
  → for each user:
      if online: codecSend
      else: OfflineMsgModel::insert
```

---

## 编译配置

**CMakeLists.txt 关键修改：**
```cmake
add_executable(chatserver
    src/server/chatserver.cpp
    src/server/chatservice.cpp
    src/db/db.cpp
    src/model/usermodel.cpp
    src/model/friendmodel.cpp       # 新增
    src/model/groupmodel.cpp        # 新增
    src/model/offlinemsgmodel.cpp   # 新增
    src/common/crypto.cpp
)
target_link_libraries(chatserver ${MUDUO_LIBS} mysqlclient crypto)
```

---

## 测试验证

### 1. 好友系统测试

```bash
# 用户 A 登录
./build/bin/chatclient
2 → 1 → 123456

# 添加用户 B 为好友
addfriend 2
# 输出：✅ 添加好友成功

# 用户 B 登录（另一个终端）
./build/bin/chatclient
2 → 2 → 123456
# 登录响应会包含好友列表（包含用户 A）
```

### 2. 一对一聊天测试

```bash
# 用户 A 发送消息给用户 B（在线）
chat 2 Hello, Bob!

# 用户 B 终端立即显示：
# [好友消息] from=1 msg: Hello, Bob!
```

### 3. 离线消息测试

```bash
# 用户 B 退出
quit

# 用户 A 发送消息
chat 2 Are you there?

# 用户 B 重新登录
2 → 2 → 123456
# 登录响应会包含离线消息
```

### 4. 群聊测试

```bash
# 用户 A 创建群组
creategroup TestGroup
# 输出：✅ 创建群组成功，群ID=1

# 用户 B 加入群组
joingroup 1
# 输出：✅ 加入群组成功

# 用户 A 发送群消息
groupchat 1 Hello, everyone!

# 用户 B 终端显示：
# [群消息] groupid=1 from=1 msg: Hello, everyone!
```

---

## 关键代码位置

- 好友模型：[src/model/friendmodel.cpp](src/model/friendmodel.cpp)
- 群组模型：[src/model/groupmodel.cpp](src/model/groupmodel.cpp)
- 离线消息模型：[src/model/offlinemsgmodel.cpp](src/model/offlinemsgmodel.cpp)
- 一对一聊天：[src/server/chatservice.cpp:255-265](src/server/chatservice.cpp#L255-L265)
- 群聊广播：[src/server/chatservice.cpp:313-330](src/server/chatservice.cpp#L313-L330)
- 登录数据加载：[src/server/chatservice.cpp:105-165](src/server/chatservice.cpp#L105-L165)
- 协议定义：[src/common/protocol.hpp](src/common/protocol.hpp)
- 客户端命令处理：[src/client/chatclient.cpp:605-710](src/client/chatclient.cpp#L605-L710)

---

## 已知限制与改进方向

### 当前限制

1. **SQL 注入风险**：使用 `snprintf` 拼接 SQL，未使用预编译语句
2. **消息格式存储**：离线消息存储完整 JSON，占用空间较大
3. **好友关系单向**：添加好友需要手动插入双向记录
4. **群组权限**：仅区分 creator 和 normal，未实现踢人、禁言等功能
5. **消息顺序**：离线消息无序号，可能乱序

### 改进方向

1. **使用预编译语句**：防止 SQL 注入
2. **消息表优化**：单独设计消息表，offlinemessage 只存消息 ID
3. **好友申请机制**：添加好友需要对方同意
4. **群组权限系统**：管理员、禁言、踢人等功能
5. **消息序号**：为消息添加全局递增 ID，保证顺序
6. **已读未读**：记录消息已读状态
7. **消息撤回**：支持消息撤回功能

---

## 交付清单

- [x] 好友系统（添加好友、查询好友列表）
- [x] 一对一聊天（在线转发 / 离线存储）
- [x] 群组功能（创建群、加入群、群聊广播）
- [x] 离线消息（存储、登录时拉取并清空）
- [x] 登录时返回好友列表、群组列表、离线消息
- [x] 客户端命令支持（chat、addfriend、creategroup、joingroup、groupchat）
- [x] 数据库表设计（friend、allgroup、groupuser、offlinemessage）
- [x] 编译通过（`make -j2 chatserver`）
- [x] README 更新到 v0.4
- [x] 验收文档（本文档）
