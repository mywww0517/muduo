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

        if not self.api_key:
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
