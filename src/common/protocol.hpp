#pragma once
#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

// 为什么用 enum 不用 #define ？
//   1. enum 有类型安全（编译器能检查）
//   2. 在调试器里能看到名字而不是数字
//   3. 现代 C++ 推荐 enum class，但普通 enum 兼容性更好
enum MsgId{
    // 错误
    ERROR_MSG       = 0,

    PING_MSG = 1,   //心跳请求
    PONG_MSG = 2,   //心跳响应

    // 注册
    REG_MSG         = 3,
    REG_MSG_ACK     = 4,

    // 登录
    LOGIN_MSG       = 5,
    LOGIN_MSG_ACK   = 6,

    // 登出
    LOGOUT_MSG      = 7,
};


#endif