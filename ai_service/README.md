# AI Service

MyMuduo Chat 的 AI 服务模块，提供 LLM 集成功能。

## 功能

- 支持多个 LLM 提供商（DeepSeek、OpenAI、Claude）
- HTTP API 接口
- 上下文管理
- 错误处理和重试

## 安装

1. 创建虚拟环境：
```bash
python3 -m venv venv
source venv/bin/activate
```

2. 安装依赖：
```bash
pip install -r requirements.txt
```

3. 配置环境变量：
```bash
cp .env.example .env
# 编辑 .env 文件，填入你的 API key
```

## 使用

### 启动服务

```bash
python app.py
```

服务将在 `http://0.0.0.0:5000` 启动。

### API 接口

#### 1. 聊天接口

**请求：**
```bash
POST /api/chat
Content-Type: application/json

{
    "message": "你好，请介绍一下自己",
    "context": [
        {"role": "user", "content": "之前的消息"},
        {"role": "assistant", "content": "之前的回复"}
    ]
}
```

**响应：**
```json
{
    "success": true,
    "response": "你好！我是 AI 助手..."
}
```

#### 2. 健康检查

**请求：**
```bash
GET /api/health
```

**响应：**
```json
{
    "status": "ok",
    "provider": "deepseek",
    "model": "deepseek-chat"
}
```

#### 3. 获取配置

**请求：**
```bash
GET /api/config
```

**响应：**
```json
{
    "provider": "deepseek",
    "model": "deepseek-chat",
    "max_context_length": 10,
    "api_base": "https://api.deepseek.com"
}
```

## 配置说明

### 环境变量

| 变量名 | 说明 | 默认值 |
|--------|------|--------|
| LLM_PROVIDER | LLM 提供商 | deepseek |
| DEEPSEEK_API_KEY | DeepSeek API key | - |
| DEEPSEEK_API_BASE | DeepSeek API 地址 | https://api.deepseek.com |
| DEEPSEEK_MODEL | DeepSeek 模型名称 | deepseek-chat |
| AI_SERVICE_PORT | 服务端口 | 5000 |
| MAX_CONTEXT_LENGTH | 最大上下文长度 | 10 |
| LLM_TIMEOUT | API 超时时间（秒） | 30 |

### 切换 LLM 提供商

修改 `.env` 文件中的 `LLM_PROVIDER`：

```bash
# 使用 DeepSeek
LLM_PROVIDER=deepseek

# 使用 OpenAI
LLM_PROVIDER=openai

# 使用 Claude
LLM_PROVIDER=claude
```

## 测试

```bash
# 测试健康检查
curl http://127.0.0.1:5000/api/health

# 测试聊天接口
curl -X POST http://127.0.0.1:5000/api/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "你好", "context": []}'
```

## 故障排查

### API key 未设置

如果看到警告 "API key 未设置"，请检查：
1. `.env` 文件是否存在
2. API key 是否正确填写
3. 环境变量是否正确加载

### API 调用失败

检查：
1. 网络连接是否正常
2. API key 是否有效
3. API 配额是否用完
4. API base URL 是否正确

## 开发

### 添加新的 LLM 提供商

1. 在 `config.py` 中添加配置
2. 在 `llm_client.py` 中实现 `_call_xxx()` 方法
3. 更新 `chat()` 方法的分发逻辑

### 日志

日志级别可以通过修改 `app.py` 中的 `logging.basicConfig(level=logging.INFO)` 来调整。

## 许可证

与 MyMuduo Chat 主项目相同
