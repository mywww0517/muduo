import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    def __init__(self):
        # LLM 提供商配置
        self.llm_provider = os.getenv('LLM_PROVIDER', 'deepseek')

        # DeepSeek 配置
        self.deepseek_api_key = os.getenv('DEEPSEEK_API_KEY', '')
        self.deepseek_api_base = os.getenv('DEEPSEEK_API_BASE', 'https://api.deepseek.com')
        self.deepseek_model = os.getenv('DEEPSEEK_MODEL', 'deepseek-chat')

        # OpenAI 配置（可选）
        self.openai_api_key = os.getenv('OPENAI_API_KEY', '')
        self.openai_api_base = os.getenv('OPENAI_API_BASE', 'https://api.openai.com')
        self.openai_model = os.getenv('OPENAI_MODEL', 'gpt-3.5-turbo')

        # Claude 配置（可选）
        self.claude_api_key = os.getenv('CLAUDE_API_KEY', '')
        self.claude_api_base = os.getenv('CLAUDE_API_BASE', 'https://api.anthropic.com')
        self.claude_model = os.getenv('CLAUDE_MODEL', 'claude-3-sonnet-20240229')

        # 服务配置
        self.port = int(os.getenv('AI_SERVICE_PORT', 5000))
        self.max_context_length = int(os.getenv('MAX_CONTEXT_LENGTH', 10))
        self.timeout = int(os.getenv('LLM_TIMEOUT', 30))

    @property
    def api_key(self):
        """根据当前提供商返回对应的 API key"""
        if self.llm_provider == 'deepseek':
            return self.deepseek_api_key
        elif self.llm_provider == 'openai':
            return self.openai_api_key
        elif self.llm_provider == 'claude':
            return self.claude_api_key
        return ''

    @property
    def api_base(self):
        """根据当前提供商返回对应的 API base URL"""
        if self.llm_provider == 'deepseek':
            return self.deepseek_api_base
        elif self.llm_provider == 'openai':
            return self.openai_api_base
        elif self.llm_provider == 'claude':
            return self.claude_api_base
        return ''

    @property
    def model_name(self):
        """根据当前提供商返回对应的模型名称"""
        if self.llm_provider == 'deepseek':
            return self.deepseek_model
        elif self.llm_provider == 'openai':
            return self.openai_model
        elif self.llm_provider == 'claude':
            return self.claude_model
        return ''
