from flask import Flask, request, jsonify
from llm_client import LLMClient
from config import Config
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)
config = Config()
llm_client = LLMClient(config)

@app.route('/api/chat', methods=['POST'])
def chat():
    """
    接收聊天消息，返回 AI 回复

    请求格式:
    {
        "message": "用户消息",
        "context": [
            {"role": "user", "content": "历史消息1"},
            {"role": "assistant", "content": "AI回复1"}
        ]
    }

    响应格式:
    {
        "success": true,
        "response": "AI 回复内容"
    }
    """
    try:
        data = request.json
        if not data:
            return jsonify({
                'success': False,
                'error': '请求体不能为空'
            }), 400

        user_message = data.get('message')
        if not user_message:
            return jsonify({
                'success': False,
                'error': 'message 字段不能为空'
            }), 400

        context = data.get('context', [])

        # 限制上下文长度
        if len(context) > config.max_context_length * 2:
            context = context[-(config.max_context_length * 2):]

        logger.info(f"收到消息: {user_message[:50]}...")

        # 调用 LLM
        response = llm_client.chat(user_message, context)

        logger.info(f"AI 回复: {response[:50]}...")

        return jsonify({
            'success': True,
            'response': response
        })

    except Exception as e:
        logger.error(f"处理请求失败: {e}")
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

@app.route('/api/health', methods=['GET'])
def health():
    """健康检查接口"""
    return jsonify({
        'status': 'ok',
        'provider': config.llm_provider,
        'model': config.model_name
    })

@app.route('/api/config', methods=['GET'])
def get_config():
    """获取当前配置（不包含敏感信息）"""
    return jsonify({
        'provider': config.llm_provider,
        'model': config.model_name,
        'max_context_length': config.max_context_length,
        'api_base': config.api_base
    })

if __name__ == '__main__':
    logger.info(f"启动 AI 服务...")
    logger.info(f"LLM 提供商: {config.llm_provider}")
    logger.info(f"模型: {config.model_name}")
    logger.info(f"端口: {config.port}")

    if not config.api_key:
        logger.warning("警告: API key 未设置，请检查 .env 配置文件")

    app.run(host='0.0.0.0', port=config.port, debug=False)
