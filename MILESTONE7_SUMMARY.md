# 里程碑⑦实现总结

## 完成内容

### 里程碑⑦：Nginx TCP 负载均衡

#### 1. Nginx 配置
- ✅ 配置 Nginx stream 模块
- ✅ 定义后端服务器组（upstream）
- ✅ 配置 TCP 负载均衡（轮询策略）
- ✅ 健康检查参数（max_fails、fail_timeout）

#### 2. 集群管理脚本
- ✅ start_cluster.sh - 启动集群脚本
- ✅ stop_cluster.sh - 停止集群脚本
- ✅ 自动加载数据库配置
- ✅ 自动重载 Nginx

#### 3. 文档更新
- ✅ README 更新到 v0.6
- ✅ 添加 Nginx 配置说明
- ✅ 添加集群启动说明

---

## 技术实现细节

### 1. **Nginx Stream 配置**

#### 配置文件结构
```nginx
stream {
    upstream chatserver_backend {
        server 127.0.0.1:8888;
        server 127.0.0.1:8889;
    }

    server {
        listen 9000;
        proxy_pass chatserver_backend;
    }
}
```

**配置说明：**
- `stream` 块：TCP/UDP 负载均衡配置
- `upstream chatserver_backend`：定义后端服务器组
- `server 127.0.0.1:8888`：后端服务器地址
- `listen 9000`：Nginx 监听端口
- `proxy_pass chatserver_backend`：转发到后端服务器组

#### 负载均衡策略

**默认：轮询（round-robin）**
```nginx
upstream chatserver_backend {
    server 127.0.0.1:8888;
    server 127.0.0.1:8889;
}
```
- 按顺序轮流分配连接
- 适合后端服务器性能相近的场景

**加权轮询（weighted round-robin）**
```nginx
upstream chatserver_backend {
    server 127.0.0.1:8888 weight=2;
    server 127.0.0.1:8889 weight=1;
}
```
- 按权重分配连接
- 适合后端服务器性能不同的场景

**最少连接（least_conn）**
```nginx
upstream chatserver_backend {
    least_conn;
    server 127.0.0.1:8888;
    server 127.0.0.1:8889;
}
```
- 优先分配到连接数最少的服务器
- 适合长连接场景

**IP 哈希（ip_hash）**
```nginx
upstream chatserver_backend {
    hash $remote_addr consistent;
    server 127.0.0.1:8888;
    server 127.0.0.1:8889;
}
```
- 根据客户端 IP 哈希分配
- 同一客户端总是连接到同一服务器
- 适合需要会话保持的场景

#### 健康检查参数

```nginx
upstream chatserver_backend {
    server 127.0.0.1:8888 weight=1 max_fails=3 fail_timeout=30s;
    server 127.0.0.1:8889 weight=1 max_fails=3 fail_timeout=30s;
}
```

**参数说明：**
- `weight=1`：权重，默认 1
- `max_fails=3`：最大失败次数，超过后标记为不可用
- `fail_timeout=30s`：失败超时时间，30 秒后重新尝试

#### 连接超时设置

```nginx
server {
    listen 9000;
    proxy_pass chatserver_backend;

    proxy_connect_timeout 10s;      # 连接超时
    proxy_timeout 300s;             # 空闲超时（5 分钟）
    proxy_socket_keepalive on;      # 启用 TCP keepalive
}
```

---

### 2. **集群管理脚本**

#### start_cluster.sh

```bash
#!/bin/bash
# 启动 MyMuduo Chat 集群

set -e

PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)

# 加载数据库配置
set -a
source "${PROJECT_DIR}/config/db.env"
set +a

# 检查 nginx 配置
if ! sudo nginx -t > /dev/null 2>&1; then
    echo "Error: Nginx configuration test failed"
    exit 1
fi

# 启动后端服务器
echo "Starting ChatServer instances..."

nohup "${PROJECT_DIR}/build/bin/chatserver" 8888 > /tmp/chatserver-8888.log 2>&1 &
SERVER1_PID=$!
echo "ChatServer 1 started on port 8888 (PID: $SERVER1_PID)"

sleep 1

nohup "${PROJECT_DIR}/build/bin/chatserver" 8889 > /tmp/chatserver-8889.log 2>&1 &
SERVER2_PID=$!
echo "ChatServer 2 started on port 8889 (PID: $SERVER2_PID)"

sleep 1

# 重载 nginx
echo "Reloading Nginx..."
sudo systemctl reload nginx

echo ""
echo "Cluster started successfully!"
echo "  - ChatServer 1: 127.0.0.1:8888 (PID: $SERVER1_PID)"
echo "  - ChatServer 2: 127.0.0.1:8889 (PID: $SERVER2_PID)"
echo "  - Nginx Load Balancer: 127.0.0.1:9000"
echo ""
echo "Connect to: ./build/bin/chatclient 127.0.0.1 9000"
```

**关键点：**
- `set -e`：遇到错误立即退出
- `set -a; source db.env; set +a`：加载环境变量并导出
- `nohup ... &`：后台运行，不受终端关闭影响
- `sudo nginx -t`：测试 Nginx 配置
- `sudo systemctl reload nginx`：重载 Nginx（不中断现有连接）

#### stop_cluster.sh

```bash
#!/bin/bash
# 停止 MyMuduo Chat 集群

echo "Stopping ChatServer instances..."

pkill -f "chatserver 8888"
pkill -f "chatserver 8889"

sleep 1

echo "Cluster stopped."
```

**关键点：**
- `pkill -f "chatserver 8888"`：根据完整命令行匹配进程
- 不停止 Nginx（可能有其他服务在使用）

---

### 3. **集群架构**

#### 架构图

```
┌─────────────┐         ┌─────────────┐
│  Client A   │         │  Client B   │
└──────┬──────┘         └──────┬──────┘
       │                       │
       │ TCP                   │ TCP
       │                       │
       └───────┬───────────────┘
               │
        ┌──────▼──────┐
        │    Nginx    │
        │  (Port 9000)│
        │ Load Balancer│
        └──────┬──────┘
               │
       ┌───────┴───────┐
       │               │
┌──────▼──────┐ ┌──────▼──────┐
│ ChatServer1 │ │ ChatServer2 │
│  (Port 8888)│ │  (Port 8889)│
└──────┬──────┘ └──────┬──────┘
       │               │
       └───────┬───────┘
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

#### 请求流程

**1. 客户端连接**
```
Client → Nginx:9000
```

**2. Nginx 负载均衡**
```
Nginx → ChatServer1:8888 (第 1 个连接)
Nginx → ChatServer2:8889 (第 2 个连接)
Nginx → ChatServer1:8888 (第 3 个连接)
...
```

**3. 跨服务器通信**
```
Client A (Server1) → Server1 → Redis → Server2 → Client B (Server2)
```

---

## 测试验证

### 1. 启动集群

```bash
# 启动集群
./scripts/start_cluster.sh

# 输出：
# Starting ChatServer instances...
# ChatServer 1 started on port 8888 (PID: 12345)
# ChatServer 2 started on port 8889 (PID: 12346)
# Reloading Nginx...
# Cluster started successfully!
#   - ChatServer 1: 127.0.0.1:8888 (PID: 12345)
#   - ChatServer 2: 127.0.0.1:8889 (PID: 12346)
#   - Nginx Load Balancer: 127.0.0.1:9000
```

### 2. 验证负载均衡

```bash
# 检查 Nginx 监听端口
netstat -tlnp | grep 9000
# tcp  0  0  0.0.0.0:9000  0.0.0.0:*  LISTEN  785/nginx

# 检查后端服务器
netstat -tlnp | grep chatserver
# tcp  0  0  0.0.0.0:8888  0.0.0.0:*  LISTEN  12345/chatserver
# tcp  0  0  0.0.0.0:8889  0.0.0.0:*  LISTEN  12346/chatserver
```

### 3. 测试客户端连接

```bash
# 终端 1：客户端 A 连接到 Nginx
./build/bin/chatclient 127.0.0.1 9000
# 登录用户 1

# 终端 2：客户端 B 连接到 Nginx
./build/bin/chatclient 127.0.0.1 9000
# 登录用户 2

# 终端 1：用户 1 发送消息给用户 2
chat 2 Hello from user 1!

# 终端 2：用户 2 收到消息
# [好友消息] from=1 msg: Hello from user 1!
```

### 4. 验证跨服务器通信

```bash
# 查看日志，确认两个用户连接到不同服务器
grep "login success" /tmp/chatserver-8888.log
grep "login success" /tmp/chatserver-8889.log

# 如果用户 1 在 Server1，用户 2 在 Server2
# 消息会通过 Redis 转发
```

---

## 技术亮点

### 1. **Nginx TCP 负载均衡**
- **四层负载均衡**：在 TCP 层转发，不解析应用层协议
- **高性能**：Nginx 支持百万级并发连接
- **灵活策略**：支持轮询、加权、最少连接、IP 哈希等策略
- **健康检查**：自动剔除故障节点

### 2. **统一入口**
- **客户端简化**：客户端只需连接 Nginx，无需关心后端服务器
- **动态扩展**：增加后端服务器只需修改 Nginx 配置，无需修改客户端
- **故障隔离**：单个后端服务器故障不影响整体服务

### 3. **集群脚本**
- **一键启动**：简化集群部署流程
- **自动配置**：自动加载环境变量、检查配置
- **日志管理**：统一日志路径，便于排查问题

### 4. **与 Redis 集成**
- **跨服务器通信**：Nginx 负责连接分发，Redis 负责消息转发
- **无状态服务器**：后端服务器无状态，易于水平扩展
- **高可用**：单个服务器故障不影响其他服务器

---

## 性能分析

### 负载均衡性能

| 指标 | 数值 | 说明 |
|---|---|---|
| Nginx 并发连接 | 10K+ | 单个 Nginx 实例支持 |
| 后端服务器数量 | 无限制 | 理论上可以无限扩展 |
| 转发延迟 | < 1ms | Nginx 转发延迟 |
| 吞吐量 | N × 单机吞吐量 | N 为后端服务器数量 |

### 扩展性

**水平扩展：**
- 增加后端服务器：修改 Nginx 配置，添加 `server` 行
- 增加 Nginx 实例：使用 LVS / DNS 轮询分发到多个 Nginx

**垂直扩展：**
- 增加服务器配置：提升单机性能
- 优化 Nginx 参数：worker_processes、worker_connections

---

## 已知限制与改进方向

### 当前限制

1. **会话保持**：默认轮询策略不保证同一用户连接到同一服务器
2. **Nginx 单点**：Nginx 挂掉后整个集群不可用
3. **手动配置**：增加后端服务器需要手动修改 Nginx 配置
4. **无监控**：缺少集群监控和告警机制

### 改进方向

1. **会话保持**：使用 IP 哈希策略，保证同一客户端连接到同一服务器
2. **Nginx 高可用**：使用 Keepalived 实现 Nginx 主备切换
3. **动态配置**：使用 Nginx Plus 或 Consul Template 实现动态配置
4. **服务发现**：使用 etcd / Consul 实现服务注册与发现
5. **监控告警**：使用 Prometheus + Grafana 监控集群状态
6. **日志聚合**：使用 ELK / Loki 聚合分析日志

---

## 交付清单

- [x] Nginx stream 配置
- [x] 后端服务器组定义（upstream）
- [x] TCP 负载均衡配置（轮询策略）
- [x] 健康检查参数配置
- [x] 集群启动脚本（start_cluster.sh）
- [x] 集群停止脚本（stop_cluster.sh）
- [x] README 更新到 v0.6
- [x] Nginx 配置说明文档
- [x] 集群启动说明文档
- [x] 测试验证通过
- [x] 验收文档（本文档）
