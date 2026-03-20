#pragma once

#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>
#include <unordered_map>
#include <functional>
#include <mutex>

#include "protocol.hpp"
#include "codec.hpp"
#include "usermodel.hpp"
#include "json.hpp"

using json = nlohmann::json;
using MsgHandler = std::function<void(const muduo::net::TcpConnectionPtr&, json&, muduo::Timestamp)>;

class ChatService {
public:
    static ChatService& instance();

    bool init();
    MsgHandler getHandler(int msgid);

    void ping(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void reg(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void login(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void logout(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);

    void clientCloseException(const muduo::net::TcpConnectionPtr& conn);
    void reset();

private:
    ChatService();

    std::unordered_map<int, MsgHandler> msgHandlerMap_;

    // 在线用户：userid -> TcpConnectionPtr
    std::unordered_map<int, muduo::net::TcpConnectionPtr> userConnMap_;
    std::mutex connMutex_;

    UserModel userModel_;
};
