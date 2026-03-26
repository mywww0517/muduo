#include "chatservice.hpp"
#include "crypto.hpp"
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
    msgHandlerMap_[ADD_FRIEND_MSG] = std::bind(&ChatService::addFriend, this, _1, _2, _3);
    msgHandlerMap_[CHAT_MSG] = std::bind(&ChatService::oneChat, this, _1, _2, _3);
    msgHandlerMap_[CREATE_GROUP_MSG] = std::bind(&ChatService::createGroup, this, _1, _2, _3);
    msgHandlerMap_[JOIN_GROUP_MSG] = std::bind(&ChatService::joinGroup, this, _1, _2, _3);
    msgHandlerMap_[GROUP_CHAT_MSG] = std::bind(&ChatService::groupChat, this, _1, _2, _3);
}

bool ChatService::init() {
    if (!userModel_.init()) {
        LOG_ERROR << "ChatService init failed: userModel init error";
        return false;
    }
    if (!friendModel_.init()) {
        LOG_ERROR << "ChatService init failed: friendModel init error";
        return false;
    }
    if (!groupModel_.init()) {
        LOG_ERROR << "ChatService init failed: groupModel init error";
        return false;
    }
    if (!offlineMsgModel_.init()) {
        LOG_ERROR << "ChatService init failed: offlineMsgModel init error";
        return false;
    }
    if (!redis_.connect()) {
        LOG_ERROR << "ChatService init failed: redis connect error";
        return false;
    }

    redis_.observer_channel_message(std::bind(&ChatService::handleRedisSubscribeMessage, this, std::placeholders::_1, std::placeholders::_2));

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

    if (user.id() == -1 || user.password() != sha256(pwd)) {
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

    // 订阅 Redis 频道
    redis_.subscribe(id);

    // 查询好友列表
    std::vector<User> friends = friendModel_.query(id);
    if (!friends.empty()) {
        json friendList = json::array();
        for (auto& f : friends) {
            json item;
            item["id"] = f.id();
            item["name"] = f.name();
            item["state"] = f.state();
            friendList.push_back(item);
        }
        response["friends"] = friendList;
    }

    // 查询群组列表
    std::vector<GroupInfo> groups = groupModel_.queryGroups(id);
    if (!groups.empty()) {
        json groupList = json::array();
        for (auto& g : groups) {
            json item;
            item["id"] = g.group.id();
            item["name"] = g.group.name();
            item["desc"] = g.group.desc();
            json users = json::array();
            for (auto& u : g.users) {
                json uitem;
                uitem["id"] = u.id();
                uitem["name"] = u.name();
                uitem["state"] = u.state();
                users.push_back(uitem);
            }
            item["users"] = users;
            groupList.push_back(item);
        }
        response["groups"] = groupList;
    }

    // 查询离线消息
    std::vector<std::string> offlineMsgs = offlineMsgModel_.query(id);
    if (!offlineMsgs.empty()) {
        response["offlinemsgs"] = offlineMsgs;
        offlineMsgModel_.remove(id);
    }

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

    redis_.unsubscribe(userid);

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
        redis_.unsubscribe(user.id());
        user.setState("offline");
        userModel_.updateState(user);
        LOG_INFO << "client close exception: id=" << user.id();
    }
}

void ChatService::reset() {
    userModel_.resetState();
}

void ChatService::handleRedisSubscribeMessage(int userid, const std::string& msg) {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = userConnMap_.find(userid);
    if (it != userConnMap_.end()) {
        codecSend(it->second, msg);
    }
}

void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    json response;
    response["msgid"] = ADD_FRIEND_ACK;

    if (friendModel_.insert(userid, friendid)) {
        response["errno"] = 0;
        LOG_INFO << "add friend: " << userid << " -> " << friendid;
    } else {
        response["errno"] = 1;
        response["errmsg"] = "add friend failed";
    }

    codecSend(conn, response.dump());
}

void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int toid = js["to"].get<int>();

    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end()) {
            codecSend(it->second, js.dump());
            return;
        }
    }

    User user = userModel_.query(toid);
    if (user.id() != -1 && user.state() == "online") {
        redis_.publish(toid, js.dump());
    } else {
        offlineMsgModel_.insert(toid, js.dump());
    }
}

void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    std::string name = js["groupname"];
    std::string desc = js.value("groupdesc", "");

    Group group;
    group.setName(name);
    group.setDesc(desc);

    json response;
    response["msgid"] = CREATE_GROUP_ACK;

    if (groupModel_.createGroup(group)) {
        groupModel_.addToGroup(userid, group.id(), "creator");
        response["errno"] = 0;
        response["groupid"] = group.id();
        LOG_INFO << "create group: " << group.id() << " by user " << userid;
    } else {
        response["errno"] = 1;
        response["errmsg"] = "create group failed";
    }

    codecSend(conn, response.dump());
}

void ChatService::joinGroup(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    json response;
    response["msgid"] = JOIN_GROUP_ACK;

    if (groupModel_.addToGroup(userid, groupid, "normal")) {
        response["errno"] = 0;
        LOG_INFO << "join group: user " << userid << " -> group " << groupid;
    } else {
        response["errno"] = 1;
        response["errmsg"] = "join group failed";
    }

    codecSend(conn, response.dump());
}

void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int groupid = js["groupid"].get<int>();
    int userid = js["id"].get<int>();

    std::vector<int> users = groupModel_.queryGroupUsers(groupid);

    std::lock_guard<std::mutex> lock(connMutex_);
    for (int id : users) {
        if (id == userid) continue;

        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end()) {
            codecSend(it->second, js.dump());
        } else {
            User user = userModel_.query(id);
            if (user.id() != -1 && user.state() == "online") {
                redis_.publish(id, js.dump());
            } else {
                offlineMsgModel_.insert(id, js.dump());
            }
        }
    }
}
