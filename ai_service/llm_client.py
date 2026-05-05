import requests
from typing import List, Dict
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class LLMClient:
    def __init__(self, config):
        self.provider = config.llm_provider
        self.api_key = config.api_key
        self.api_base = config.api_base
        self.model = config.model_name
        self.timeout = config.timeout
        self.mock_mode = config.mock_mode

        if not self.api_key and not self.mock_mode:
            logger.warning(f"API key for {self.provider} is not set")

    def chat(self, message: str, context: List[Dict] = None) -> str:
        """
        调用 LLM API 生成回复

        Args:
            message: 用户消息
            context: 上下文历史 [{'role': 'user', 'content': '...'}, ...]

        Returns:
            AI 回复内容
        """
        if context is None:
            context = []

        try:
            # 模拟模式：返回预设回复
            if self.mock_mode:
                return self._mock_response(message)

            if self.provider == 'deepseek':
                return self._call_deepseek(message, context)
            elif self.provider == 'openai':
                return self._call_openai(message, context)
            elif self.provider == 'claude':
                return self._call_claude(message, context)
            else:
                return f"不支持的 LLM 提供商: {self.provider}"
        except Exception as e:
            logger.error(f"LLM API 调用失败: {e}")
            return f"抱歉，我现在无法回复。错误: {str(e)}"

    def _mock_response(self, message: str) -> str:
        """模拟 AI 回复（用于测试）"""
        logger.info(f"[MOCK MODE] 收到消息: {message}")

        # 根据消息内容返回不同的模拟回复
        message_lower = message.lower()

        if "你好" in message or "hello" in message_lower or "hi" in message_lower:
            return "你好！我是 AI 助手，很高兴为你服务。我现在运行在模拟模式下，可以回答你的问题。"

        elif "介绍" in message or "你是谁" in message:
            return "我是基于 MyMuduo Chat v2.0 集成的 AI 聊天机器人。我可以在群聊中回答问题、提供帮助。当前运行在模拟模式，用于测试系统架构。"

        elif "天气" in message or "weather" in message_lower:
            return "抱歉，我目前无法查询实时天气信息。但我可以回答其他问题！"

        elif "帮助" in message or "help" in message_lower:
            return "我可以帮你：\n1. 回答技术问题\n2. 提供建议和想法\n3. 进行简单对话\n\n在群聊中 @AI 就可以呼叫我！"

        elif "测试" in message or "test" in message_lower:
            return f"测试成功！我收到了你的消息：「{message[:50]}」\n系统运行正常，AI 集成工作正常。"

        else:
            return f"我理解你说的是：「{message[:100]}」\n\n这是一个模拟回复。在实际使用中，我会调用真实的 LLM API 来生成更智能的回复。"

    def _call_deepseek(self, message: str, context: List[Dict]) -> str:
        """调用 DeepSeek API"""
        headers = {
            'Authorization': f'Bearer {self.api_key}',
            'Content-Type': 'application/json'
        }

        # 构建消息列表
        messages = context + [{'role': 'user', 'content': message}]

        payload = {
            'model': self.model,
            'messages': messages,
            'temperature': 0.7,
            'max_tokens': 2000
        }

        logger.info(f"调用 DeepSeek API: {self.api_base}/v1/chat/completions")

        response = requests.post(
            f'{self.api_base}/v1/chat/completions',
            headers=headers,
            json=payload,
            timeout=self.timeout
        )

        if response.status_code == 200:
            result = response.json()
            return result['choices'][0]['message']['content']
        else:
            logger.error(f"DeepSeek API 错误: {response.status_code} - {response.text}")
            raise Exception(f'DeepSeek API error: {response.status_code}')

    def _call_openai(self, message: str, context: List[Dict]) -> str:
        """调用 OpenAI API"""
        headers = {
            'Authorization': f'Bearer {self.api_key}',
            'Content-Type': 'application/json'
        }

        messages = context + [{'role': 'user', 'content': message}]

        payload = {
            'model': self.model,
            'messages': messages,
            'temperature': 0.7,
            'max_tokens': 2000
        }

        logger.info(f"调用 OpenAI API: {self.api_base}/v1/chat/completions")

        response = requests.post(
            f'{self.api_base}/v1/chat/completions',
            headers=headers,
            json=payload,
            timeout=self.timeout
        )

        if response.status_code == 200:
            result = response.json()
            return result['choices'][0]['message']['content']
        else:
            logger.error(f"OpenAI API 错误: {response.status_code} - {response.text}")
            raise Exception(f'OpenAI API error: {response.status_code}')

    def _call_claude(self, message: str, context: List[Dict]) -> str:
        """调用 Claude API"""
        headers = {
            'x-api-key': self.api_key,
            'anthropic-version': '2023-06-01',
            'Content-Type': 'application/json'
        }

        # Claude API 格式略有不同
        messages = context + [{'role': 'user', 'content': message}]

        payload = {
            'model': self.model,
            'messages': messages,
            'max_tokens': 2000
        }

        logger.info(f"调用 Claude API: {self.api_base}/v1/messages")

        response = requests.post(
            f'{self.api_base}/v1/messages',
            headers=headers,
            json=payload,
            timeout=self.timeout
        )

        if response.status_code == 200:
            result = response.json()
            return result['content'][0]['text']
        else:
            logger.error(f"Claude API 错误: {response.status_code} - {response.text}")
            raise Exception(f'Claude API error: {response.status_code}')
