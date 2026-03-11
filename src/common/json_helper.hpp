#ifndef JSON_HELPER_HPP
#define JSON_HELPER_HPP

// nlohmann/json 是一个"header-only"库（只有一个 .hpp 文件）
// 不需要编译链接，#include 就能用
// 官方文档：https://github.com/nlohmann/json

#include "../../thirdparty/json.hpp"

using json = nlohmann::json;

// 构造一条协议消息的 JSON 字符串
// dump() 把 json 对象转成 string
// 不加参数 = 紧凑格式（无空格无换行）
// dump(4) = 带缩进的格式（调试时好看，但网络传输浪费带宽）
inline std::string makeMessage(int msgid, const std::string &data)
{
    json j;
    j["msgid"] = msgid;
    j["data"] = data;
    return j.dump();    // 序列化为字符串
}

// 解析一条 JSON 消息
// parse:将str转换为json
inline json parseMessage(const std::string &str)
{
    return json::parse(str);
}

#endif // JSON_HELPER_HPP
