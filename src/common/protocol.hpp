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

    // 好友管理增强 (v1.0)
    DELETE_FRIEND_MSG      = 40,
    DELETE_FRIEND_ACK      = 41,
    SET_FRIEND_REMARK_MSG  = 42,
    SET_FRIEND_REMARK_ACK  = 43,
    BLOCK_FRIEND_MSG       = 44,
    BLOCK_FRIEND_ACK       = 45,
    UNBLOCK_FRIEND_MSG     = 46,
    UNBLOCK_FRIEND_ACK     = 47,

    // 群组管理增强 (v1.0)
    LEAVE_GROUP_MSG             = 50,
    LEAVE_GROUP_ACK             = 51,
    KICK_GROUP_MEMBER_MSG       = 52,
    KICK_GROUP_MEMBER_ACK       = 53,
    TRANSFER_GROUP_MSG          = 54,
    TRANSFER_GROUP_ACK          = 55,
    SET_GROUP_ANNOUNCEMENT_MSG  = 56,
    SET_GROUP_ANNOUNCEMENT_ACK  = 57,
    GET_GROUP_MEMBERS_MSG       = 58,
    GET_GROUP_MEMBERS_ACK       = 59,

    // 消息功能增强 (v1.0)
    MESSAGE_READ_ACK            = 60,
    RECALL_MESSAGE_MSG          = 61,
    RECALL_MESSAGE_ACK          = 62,
    QUERY_HISTORY_MSG           = 63,
    QUERY_HISTORY_ACK           = 64,

    // AI 功能 (v2.0)
    AI_CHAT_MSG                 = 70,
};

#endif