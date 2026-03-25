# 里程碑③验收文档

## 完成内容

### 1. 密码哈希（SHA256）
- ✅ 新增 `src/common/crypto.hpp` 和 `src/common/crypto.cpp`
- ✅ 使用 OpenSSL SHA256 对密码进行哈希
- ✅ 注册时自动哈希密码后存储
- ✅ 登录时对输入密码哈希后与数据库比对

### 2. 数据库更新
- ✅ 将 `user.password` 字段从 VARCHAR(50) 扩展到 VARCHAR(64)
- ✅ 支持存储 64 字符的 SHA256 十六进制哈希值

### 3. 配置管理
- ✅ 已有 `config/db.env` 和 `config/db.env.example`
- ✅ 通过环境变量读取数据库配置（无硬编码密码）
- ✅ 更新 `scripts/run_server.sh` 自动加载环境变量

### 4. 在线状态管理
- ✅ 内存维护 `userConnMap_`（userId -> TcpConnectionPtr）
- ✅ 数据库 `user.state` 字段（online/offline）
- ✅ 登录时检查重复登录（返回错误码 2）
- ✅ 断线时自动清理（`clientCloseException`）
- ✅ 显式登出时清理状态（`logout`）
- ✅ 服务器启动时重置所有用户为 offline（`reset`）

### 5. 编译配置
- ✅ 更新 CMakeLists.txt 链接 `crypto` 库（OpenSSL）
- ✅ 编译通过：`cmake .. && make -j2 chatserver`

### 6. 文档更新
- ✅ README.md 更新数据库建表 SQL（password VARCHAR(64)）
- ✅ 添加依赖说明（libssl-dev）
- ✅ 更新版本说明（密码采用 SHA256 哈希）
- ✅ 添加注册/登录流程说明（密码自动哈希）

---

## 验收测试

### 数据库准备

```sql
-- 1. 确认表结构
USE chat;
DESCRIBE user;
-- password 字段应为 VARCHAR(64)

-- 2. 清空测试数据（可选）
DELETE FROM user WHERE name LIKE 'test%';
```

### 启动服务器

```bash
cd /root/mymuduo
./scripts/run_server.sh
```

### 测试注册

**客户端操作：**
```bash
./build/bin/chatclient
# 选择 1 (register)
# 输入用户名：testuser
# 输入密码：test123
```

**预期结果：**
- 客户端显示：`✅ 注册成功！你的 ID = X`
- 数据库验证：
```sql
SELECT id, name, LENGTH(password) as pwd_len, state FROM user WHERE name='testuser';
-- pwd_len 应为 64（SHA256 哈希长度）
-- state 应为 offline
```

### 测试登录

**客户端操作：**
```bash
# 选择 2 (login)
# 输入 ID：X（上一步返回的 ID）
# 输入密码：test123
```

**预期结果：**
- 客户端显示：`✅ 登录成功！欢迎 testuser (id=X)`
- 数据库验证：
```sql
SELECT id, name, state FROM user WHERE id=X;
-- state 应为 online
```

### 测试重复登录

**操作：**
- 保持第一个客户端登录状态
- 启动第二个客户端，用相同 ID 登录

**预期结果：**
- 第二个客户端收到错误：`errno=2, errmsg="user already online"`

### 测试断线清理

**操作：**
- 客户端登录后直接 Ctrl+C 强制退出（不发送 logout）

**预期结果：**
- 服务器日志显示：`client close exception: id=X`
- 数据库验证：
```sql
SELECT state FROM user WHERE id=X;
-- state 应为 offline
```

### 测试显式登出

**操作：**
- 客户端登录后输入 `logout`

**预期结果：**
- 客户端回到未登录状态
- 数据库 state 变为 offline

---

## 技术细节

### 密码哈希实现
```cpp
// crypto.cpp
std::string sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}
```

### 注册流程
```cpp
// usermodel.cpp
bool UserModel::insert(User& user) {
    std::string hashedPwd = sha256(user.password());  // 哈希密码
    // ... 存储 hashedPwd 到数据库
}
```

### 登录验证
```cpp
// chatservice.cpp
void ChatService::login(...) {
    User user = userModel_.query(id);
    if (user.password() != sha256(pwd)) {  // 比对哈希值
        // 密码错误
    }
}
```

---

## 已知限制

1. **SQL 注入风险**：当前使用 `snprintf` 拼接 SQL，未使用预编译语句
2. **无加盐哈希**：SHA256 未加盐，相同密码哈希值相同（彩虹表攻击风险）
3. **重复登录策略**：仅拒绝，未实现踢线功能
4. **心跳机制**：PING_MSG 已定义但未用于保活检测

这些可在后续里程碑中改进。

---

## 交付清单

- [x] 能注册新用户（写入 MySQL，密码 SHA256 哈希）
- [x] 能登录（校验成功返回 userId/name；失败有明确错误码）
- [x] 登录后服务端记录在线（内存 + 数据库）
- [x] 断开连接自动下线
- [x] 重复登录有处理策略（返回 errno=2）
- [x] README 增加数据库建表 SQL + 配置方法 + 验证说明
- [x] 编译通过（`make -j2 chatserver`）
