#include "chatservice.hpp"
#include <muduo/base/Logging.h>
#include <functional>

using namespace std::placeholders;
using muduo::Timestamp;
using muduo::net::TcpConnectionPtr;

ChatService& ChatService::instance() {
    static ChatService service;
    return service;
}

ChatService::ChatService() {
    msgHandlerMap_[PING_MSG]   = std::bind(&ChatService::ping, this, _1, _2, _3);
    msgHandlerMap_[REG_MSG]    = std::bind(&ChatService::reg, this, _1, _2, _3);
    msgHandlerMap_[LOGIN_MSG]  = std::bind(&ChatService::login, this, _1, _2, _3);
    msgHandlerMap_[LOGOUT_MSG] = std::bind(&ChatService::logout, this, _1, _2, _3);
}

bool ChatService::init() {
    if (!userModel_.init()) {
        LOG_ERROR << "ChatService init failed: MySQL connect error";
        return false;
    }
    return true;
}

MsgHandler ChatService::getHandler(int msgid) {
    auto it = msgHandlerMap_.find(msgid);
    if (it == msgHandlerMap_.end()) {
        return [msgid](const TcpConnectionPtr& conn, json&, Timestamp) {
            LOG_ERROR << "msgid=" << msgid << " has no handler";

            json response;
            response["msgid"] = ERROR_MSG;
            response["error"] = "unknown msgid: " + std::to_string(msgid);
            codecSend(conn, response.dump());
        };
    }
    return it->second;
}

void ChatService::ping(const TcpConnectionPtr& conn, json&, Timestamp) {
    json response;
    response["msgid"] = PONG_MSG;
    response["data"]  = "pong";
    codecSend(conn, response.dump());
}

void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp) {
    std::string name = js["name"];
    std::string pwd  = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);

    json response;
    response["msgid"] = REG_MSG_ACK;

    if (userModel_.insert(user)) {
        response["errno"] = 0;
        response["id"]    = user.id();
        LOG_INFO << "register success: name=" << name << " id=" << user.id();
    } else {
        response["errno"]  = 1;
        response["errmsg"] = "register failed, name already exists";
        LOG_WARN << "register failed: name=" << name;
    }

    codecSend(conn, response.dump());
}

void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int id = js["id"].get<int>();
    std::string pwd = js["password"];

    User user = userModel_.query(id);

    json response;
    response["msgid"] = LOGIN_MSG_ACK;

    if (user.id() == -1 || user.password() != pwd) {
        response["errno"]  = 1;
        response["errmsg"] = "id or password error";
        LOG_WARN << "login failed: id=" << id;
        codecSend(conn, response.dump());
        return;
    }

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        if (userConnMap_.count(id) > 0 || user.state() == "online") {
            response["errno"]  = 2;
            response["errmsg"] = "user already online";
            LOG_WARN << "login failed: id=" << id << " already online";
            codecSend(conn, response.dump());
            return;
        }

        userConnMap_[id] = conn;
    }

    user.setState("online");
    if (!userModel_.updateState(user)) {
        std::lock_guard<std::mutex> lock(connMutex_);
        userConnMap_.erase(id);

        response["errno"]  = 1;
        response["errmsg"] = "database error";
        LOG_ERROR << "login updateState failed: id=" << id;
        codecSend(conn, response.dump());
        return;
    }

    response["errno"] = 0;
    response["id"]    = user.id();
    response["name"]  = user.name();
    LOG_INFO << "login success: id=" << id << " name=" << user.name();

    codecSend(conn, response.dump());
}

void ChatService::logout(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    bool removed = false;

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = userConnMap_.find(userid);
        if (it != userConnMap_.end() && it->second == conn) {
            userConnMap_.erase(it);
            removed = true;
        }
    }

    if (!removed) {
        LOG_WARN << "logout ignored: id=" << userid << " not matched with current connection";
        return;
    }

    User user(userid);
    user.setState("offline");
    userModel_.updateState(user);

    LOG_INFO << "logout: id=" << userid;
}

void ChatService::clientCloseException(const TcpConnectionPtr& conn) {
    User user;

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        for (auto it = userConnMap_.begin(); it != userConnMap_.end(); ++it) {
            if (it->second == conn) {
                user.setId(it->first);
                userConnMap_.erase(it);
                break;
            }
        }
    }

    if (user.id() != -1) {
        user.setState("offline");
        userModel_.updateState(user);
        LOG_INFO << "client close exception: id=" << user.id();
    }
}

void ChatService::reset() {
    userModel_.resetState();
}
