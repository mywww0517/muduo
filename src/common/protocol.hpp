#pragma once
#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

enum MsgId{
    ERROR_MSG       = 0,

    PING_MSG = 1,
    PONG_MSG = 2,

    REG_MSG         = 3,
    REG_MSG_ACK     = 4,

    LOGIN_MSG       = 5,
    LOGIN_MSG_ACK   = 6,

    LOGOUT_MSG      = 7,

    // 好友
    ADD_FRIEND_MSG  = 10,
    ADD_FRIEND_ACK  = 11,

    // 一对一聊天
    CHAT_MSG        = 20,

    // 群组
    CREATE_GROUP_MSG = 30,
    CREATE_GROUP_ACK = 31,
    JOIN_GROUP_MSG   = 32,
    JOIN_GROUP_ACK   = 33,
    GROUP_CHAT_MSG   = 34,
};

#endif