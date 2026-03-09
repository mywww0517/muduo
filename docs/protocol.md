# 通信协议 v0.1

## 消息格式

所有消息使用 JSON 文本格式，UTF-8 编码。

### 基本结构

```json
{
    "msgid": <int>,
    "data": <string 或 object>
}
