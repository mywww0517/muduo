#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

// 为什么用 enum 不用 #define ？
//   1. enum 有类型安全（编译器能检查）
//   2. 在调试器里能看到名字而不是数字
//   3. 现代 C++ 推荐 enum class，但普通 enum 兼容性更好
enum MsgId{
    PING_MSG = 1,   //心跳请求
    PONG_MSG = 2,   //心跳响应

    // ---- 后续扩展 ----
    // LOGIN_REQ = 3,
    // LOGIN_RSP = 4,
    // REG_REQ   = 5,
    // REG_RSP   = 6,
    // CHAT_MSG  = 7,
    // CHAT_MSG_ACK = 8,
};


#endif