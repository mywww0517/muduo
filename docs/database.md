# 数据库设计

## 数据库

- 名称：chat
- 字符集：utf8mb4

## user 表

| 字段     | 类型                          | 说明 |
|----------|-------------------------------|------|
| id       | INT AUTO_INCREMENT PRIMARY KEY | 用户 ID |
| name     | VARCHAR(50) NOT NULL UNIQUE   | 用户名，唯一 |
| password | VARCHAR(50) NOT NULL          | 密码（当前明文存储） |
| state    | ENUM('online','offline')      | 在线状态，默认 offline |

## 建表语句

```sql
CREATE DATABASE IF NOT EXISTS chat DEFAULT CHARSET utf8mb4;
USE chat;

CREATE TABLE IF NOT EXISTS user (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(50) NOT NULL,
    state ENUM('online', 'offline') DEFAULT 'offline'
);
```

## 开发环境应用账号

```sql
CREATE USER IF NOT EXISTS 'chatapp'@'localhost' IDENTIFIED BY '你的本地密码';
CREATE USER IF NOT EXISTS 'chatapp'@'127.0.0.1' IDENTIFIED BY '你的本地密码';

GRANT SELECT, INSERT, UPDATE, DELETE ON chat.* TO 'chatapp'@'localhost';
GRANT SELECT, INSERT, UPDATE, DELETE ON chat.* TO 'chatapp'@'127.0.0.1';

FLUSH PRIVILEGES;
```

## 配置方式

- 数据库连接信息通过环境变量读取
- 本地真实配置文件为 `config/db.env`
- 仓库只提交 `config/db.env.example`
- 禁止把真实密码提交到 GitHub

## 备注

- 当前密码明文存储，后续可升级为 bcrypt + salt
- 后续会增加 friend 表、allgroup 表、offlinemessage 表
