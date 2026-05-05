#include "chatservice.hpp"
#include "crypto.hpp"
#include <muduo/base/Logging.h>
#include <functional>
#include <thread>
#include <curl/curl.h>

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

    // v1.0 好友管理增强
    msgHandlerMap_[DELETE_FRIEND_MSG] = std::bind(&ChatService::deleteFriend, this, _1, _2, _3);
    msgHandlerMap_[SET_FRIEND_REMARK_MSG] = std::bind(&ChatService::setFriendRemark, this, _1, _2, _3);
    msgHandlerMap_[BLOCK_FRIEND_MSG] = std::bind(&ChatService::blockFriend, this, _1, _2, _3);
    msgHandlerMap_[UNBLOCK_FRIEND_MSG] = std::bind(&ChatService::unblockFriend, this, _1, _2, _3);

    // v1.0 群组管理增强
    msgHandlerMap_[LEAVE_GROUP_MSG] = std::bind(&ChatService::leaveGroup, this, _1, _2, _3);
    msgHandlerMap_[KICK_GROUP_MEMBER_MSG] = std::bind(&ChatService::kickGroupMember, this, _1, _2, _3);
    msgHandlerMap_[TRANSFER_GROUP_MSG] = std::bind(&ChatService::transferGroup, this, _1, _2, _3);
    msgHandlerMap_[SET_GROUP_ANNOUNCEMENT_MSG] = std::bind(&ChatService::setGroupAnnouncement, this, _1, _2, _3);
    msgHandlerMap_[GET_GROUP_MEMBERS_MSG] = std::bind(&ChatService::getGroupMembers, this, _1, _2, _3);

    // v1.0 消息功能增强
    msgHandlerMap_[RECALL_MESSAGE_MSG] = std::bind(&ChatService::recallMessage, this, _1, _2, _3);
    msgHandlerMap_[QUERY_HISTORY_MSG] = std::bind(&ChatService::queryHistory, this, _1, _2, _3);
    msgHandlerMap_[MESSAGE_READ_ACK] = std::bind(&ChatService::messageReadAck, this, _1, _2, _3);
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
    if (!messageModel_.init()) {
        LOG_ERROR << "ChatService init failed: messageModel init error";
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
    int fromid = js["from"].get<int>();
    int toid = js["to"].get<int>();
    std::string msg = js["msg"];

    // 检查是否被对方拉黑
    if (friendModel_.isBlocked(fromid, toid)) {
        json response;
        response["msgid"] = ERROR_MSG;
        response["errno"] = 1;
        response["errmsg"] = "message blocked by receiver";
        codecSend(conn, response.dump());
        LOG_WARN << "message blocked: from=" << fromid << " to=" << toid;
        return;
    }

    // 存储消息到数据库
    long long message_id = messageModel_.insertMessage(fromid, toid, "private", msg);
    if (message_id > 0) {
        js["message_id"] = message_id;
    }

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
    std::string msg = js["msg"];

    // 存储群聊消息到数据库
    long long message_id = messageModel_.insertMessage(userid, groupid, "group", msg);
    if (message_id > 0) {
        js["message_id"] = message_id;
    }

    // 处理 @提及功能
    std::vector<int> mentioned_users;
    if (js.contains("mentioned_users")) {
        mentioned_users = js["mentioned_users"].get<std::vector<int>>();
    }

    // 检测是否 @AI (v2.0)
    bool mentionAI = (msg.find("@AI") != std::string::npos ||
                      std::find(mentioned_users.begin(), mentioned_users.end(), 999999) != mentioned_users.end());

    std::vector<int> users = groupModel_.queryGroupUsers(groupid);

    std::lock_guard<std::mutex> lock(connMutex_);
    for (int id : users) {
        if (id == userid) continue;

        // 如果该用户被 @，在消息中添加标记
        json send_js = js;
        if (std::find(mentioned_users.begin(), mentioned_users.end(), id) != mentioned_users.end()) {
            send_js["mentioned"] = true;
        }

        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end()) {
            codecSend(it->second, send_js.dump());
        } else {
            User user = userModel_.query(id);
            if (user.id() != -1 && user.state() == "online") {
                redis_.publish(id, send_js.dump());
            } else {
                offlineMsgModel_.insert(id, send_js.dump());
            }
        }
    }

    // 如果 @AI，触发 AI 回复 (v2.0)
    if (mentionAI) {
        handleAIRequest(groupid, userid, msg);
    }
}

// v1.0 好友管理增强功能实现

// v1.0 好友管理增强功能实现

void ChatService::deleteFriend(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    json response;
    response["msgid"] = DELETE_FRIEND_ACK;

    if (friendModel_.deleteFriend(userid, friendid)) {
        response["errno"] = 0;
        LOG_INFO << "delete friend: " << userid << " <-> " << friendid;
    } else {
        response["errno"] = 1;
        response["errmsg"] = "delete friend failed";
        LOG_WARN << "delete friend failed: " << userid << " <-> " << friendid;
    }

    codecSend(conn, response.dump());
}

void ChatService::setFriendRemark(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    std::string remark = js["remark"].get<std::string>();

    json response;
    response["msgid"] = SET_FRIEND_REMARK_ACK;

    if (friendModel_.setFriendRemark(userid, friendid, remark)) {
        response["errno"] = 0;
        LOG_INFO << "set friend remark: user=" << userid << " friend=" << friendid << " remark=" << remark;
    } else {
        response["errno"] = 1;
        response["errmsg"] = "set friend remark failed";
        LOG_WARN << "set friend remark failed: user=" << userid << " friend=" << friendid;
    }

    codecSend(conn, response.dump());
}

void ChatService::blockFriend(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    json response;
    response["msgid"] = BLOCK_FRIEND_ACK;

    if (friendModel_.blockFriend(userid, friendid)) {
        response["errno"] = 0;
        LOG_INFO << "block friend: user=" << userid << " blocked=" << friendid;
    } else {
        response["errno"] = 1;
        response["errmsg"] = "block friend failed";
        LOG_WARN << "block friend failed: user=" << userid << " friend=" << friendid;
    }

    codecSend(conn, response.dump());
}

void ChatService::unblockFriend(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    json response;
    response["msgid"] = UNBLOCK_FRIEND_ACK;

    if (friendModel_.unblockFriend(userid, friendid)) {
        response["errno"] = 0;
        LOG_INFO << "unblock friend: user=" << userid << " unblocked=" << friendid;
    } else {
        response["errno"] = 1;
        response["errmsg"] = "unblock friend failed";
        LOG_WARN << "unblock friend failed: user=" << userid << " friend=" << friendid;
    }

    codecSend(conn, response.dump());
}

// v1.0 群组管理增强功能实现

void ChatService::leaveGroup(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    json response;
    response["msgid"] = LEAVE_GROUP_ACK;

    // 检查用户是否为群主
    std::string role = groupModel_.getUserRole(groupid, userid);
    if (role == "creator") {
        response["errno"] = 1;
        response["errmsg"] = "creator cannot leave group, please transfer group first";
        LOG_WARN << "leave group failed: user=" << userid << " is creator of group=" << groupid;
        codecSend(conn, response.dump());
        return;
    }

    if (groupModel_.leaveGroup(userid, groupid)) {
        response["errno"] = 0;
        LOG_INFO << "leave group: user=" << userid << " left group=" << groupid;

        // 广播退群通知
        User user = userModel_.query(userid);
        std::string notification = user.name() + " left the group";
        broadcastGroupNotification(groupid, notification);
    } else {
        response["errno"] = 1;
        response["errmsg"] = "leave group failed";
        LOG_WARN << "leave group failed: user=" << userid << " group=" << groupid;
    }

    codecSend(conn, response.dump());
}

void ChatService::kickGroupMember(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int operatorid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    int targetid = js["targetid"].get<int>();

    json response;
    response["msgid"] = KICK_GROUP_MEMBER_ACK;

    if (groupModel_.kickMember(groupid, operatorid, targetid)) {
        response["errno"] = 0;
        LOG_INFO << "kick member: operator=" << operatorid << " kicked user=" << targetid << " from group=" << groupid;

        // 通知被踢用户
        json kickNotify;
        kickNotify["msgid"] = KICK_GROUP_MEMBER_ACK;
        kickNotify["groupid"] = groupid;
        kickNotify["kicked"] = true;
        kickNotify["msg"] = "You have been removed from the group";

        {
            std::lock_guard<std::mutex> lock(connMutex_);
            auto it = userConnMap_.find(targetid);
            if (it != userConnMap_.end()) {
                codecSend(it->second, kickNotify.dump());
            } else {
                User user = userModel_.query(targetid);
                if (user.id() != -1 && user.state() == "online") {
                    redis_.publish(targetid, kickNotify.dump());
                } else {
                    offlineMsgModel_.insert(targetid, kickNotify.dump());
                }
            }
        }

        // 广播踢人通知给其他成员
        User targetUser = userModel_.query(targetid);
        std::string notification = targetUser.name() + " was removed from the group";
        broadcastGroupNotification(groupid, notification);
    } else {
        response["errno"] = 1;
        response["errmsg"] = "kick member failed, only creator can kick members";
        LOG_WARN << "kick member failed: operator=" << operatorid << " target=" << targetid << " group=" << groupid;
    }

    codecSend(conn, response.dump());
}

void ChatService::transferGroup(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int oldcreator = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    int newcreator = js["newcreator"].get<int>();

    json response;
    response["msgid"] = TRANSFER_GROUP_ACK;

    if (groupModel_.transferGroup(groupid, oldcreator, newcreator)) {
        response["errno"] = 0;
        LOG_INFO << "transfer group: group=" << groupid << " from user=" << oldcreator << " to user=" << newcreator;

        // 广播群主转让通知
        User oldUser = userModel_.query(oldcreator);
        User newUser = userModel_.query(newcreator);
        std::string notification = "Group ownership transferred from " + oldUser.name() + " to " + newUser.name();
        broadcastGroupNotification(groupid, notification);
    } else {
        response["errno"] = 1;
        response["errmsg"] = "transfer group failed, only creator can transfer group";
        LOG_WARN << "transfer group failed: old=" << oldcreator << " new=" << newcreator << " group=" << groupid;
    }

    codecSend(conn, response.dump());
}

void ChatService::setGroupAnnouncement(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    std::string announcement = js["announcement"].get<std::string>();

    json response;
    response["msgid"] = SET_GROUP_ANNOUNCEMENT_ACK;

    // 验证用户是否为群主
    std::string role = groupModel_.getUserRole(groupid, userid);
    if (role != "creator") {
        response["errno"] = 1;
        response["errmsg"] = "only creator can set announcement";
        LOG_WARN << "set announcement failed: user=" << userid << " is not creator of group=" << groupid;
        codecSend(conn, response.dump());
        return;
    }

    if (groupModel_.setAnnouncement(groupid, announcement)) {
        response["errno"] = 0;
        LOG_INFO << "set announcement: group=" << groupid << " by user=" << userid;

        // 广播群公告更新通知
        std::string notification = "Group announcement updated: " + announcement;
        broadcastGroupNotification(groupid, notification);
    } else {
        response["errno"] = 1;
        response["errmsg"] = "set announcement failed";
        LOG_WARN << "set announcement failed: group=" << groupid << " user=" << userid;
    }

    codecSend(conn, response.dump());
}

void ChatService::getGroupMembers(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    json response;
    response["msgid"] = GET_GROUP_MEMBERS_ACK;

    // 验证用户是否为群成员
    std::string role = groupModel_.getUserRole(groupid, userid);
    if (role.empty()) {
        response["errno"] = 1;
        response["errmsg"] = "you are not a member of this group";
        LOG_WARN << "get members failed: user=" << userid << " not in group=" << groupid;
        codecSend(conn, response.dump());
        return;
    }

    std::vector<GroupUser> groupUsers = groupModel_.queryGroupUsersWithDetail(groupid);
    json members = json::array();

    for (auto& gu : groupUsers) {
        User user = userModel_.query(gu.userId());
        if (user.id() != -1) {
            json member;
            member["id"] = user.id();
            member["name"] = user.name();
            member["state"] = user.state();
            member["role"] = gu.role();
            members.push_back(member);
        }
    }

    response["errno"] = 0;
    response["members"] = members;
    LOG_INFO << "get members: group=" << groupid << " by user=" << userid;

    codecSend(conn, response.dump());
}

void ChatService::broadcastGroupNotification(int groupid, const std::string& notification) {
    std::vector<int> users = groupModel_.queryGroupUsers(groupid);

    json notifyMsg;
    notifyMsg["msgid"] = GROUP_CHAT_MSG;
    notifyMsg["groupid"] = groupid;
    notifyMsg["id"] = 0;  // 系统消息
    notifyMsg["name"] = "System";
    notifyMsg["msg"] = notification;
    notifyMsg["time"] = std::time(nullptr);

    std::lock_guard<std::mutex> lock(connMutex_);
    for (int id : users) {
        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end()) {
            codecSend(it->second, notifyMsg.dump());
        } else {
            User user = userModel_.query(id);
            if (user.id() != -1 && user.state() == "online") {
                redis_.publish(id, notifyMsg.dump());
            } else {
                offlineMsgModel_.insert(id, notifyMsg.dump());
            }
        }
    }
}

// v1.0 消息功能增强实现

void ChatService::recallMessage(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    long long message_id = js["message_id"].get<long long>();

    json response;
    response["msgid"] = RECALL_MESSAGE_ACK;

    if (messageModel_.recallMessage(message_id, userid)) {
        response["errno"] = 0;
        response["message_id"] = message_id;
        LOG_INFO << "recall message: id=" << message_id << " by user=" << userid;

        // 查询消息信息，通知相关用户
        // 这里简化处理，实际应该查询消息的接收者并发送撤回通知
        json notifyMsg;
        notifyMsg["msgid"] = RECALL_MESSAGE_MSG;
        notifyMsg["message_id"] = message_id;
        notifyMsg["from"] = userid;

        // 如果是私聊消息，通知对方
        if (js.contains("to")) {
            int toid = js["to"].get<int>();
            std::lock_guard<std::mutex> lock(connMutex_);
            auto it = userConnMap_.find(toid);
            if (it != userConnMap_.end()) {
                codecSend(it->second, notifyMsg.dump());
            } else {
                User user = userModel_.query(toid);
                if (user.id() != -1 && user.state() == "online") {
                    redis_.publish(toid, notifyMsg.dump());
                }
            }
        }
        // 如果是群聊消息，通知群成员
        else if (js.contains("groupid")) {
            int groupid = js["groupid"].get<int>();
            notifyMsg["groupid"] = groupid;
            std::vector<int> users = groupModel_.queryGroupUsers(groupid);

            std::lock_guard<std::mutex> lock(connMutex_);
            for (int id : users) {
                if (id == userid) continue;

                auto it = userConnMap_.find(id);
                if (it != userConnMap_.end()) {
                    codecSend(it->second, notifyMsg.dump());
                } else {
                    User user = userModel_.query(id);
                    if (user.id() != -1 && user.state() == "online") {
                        redis_.publish(id, notifyMsg.dump());
                    }
                }
            }
        }
    } else {
        response["errno"] = 1;
        response["errmsg"] = "recall failed (超过2分钟或非本人消息)";
        LOG_WARN << "recall message failed: id=" << message_id << " user=" << userid;
    }

    codecSend(conn, response.dump());
}

void ChatService::queryHistory(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    int chat_id = js["chat_id"].get<int>();
    std::string msg_type = js["msg_type"].get<std::string>();
    int offset = js.value("offset", 0);
    int limit = js.value("limit", 20);

    json response;
    response["msgid"] = QUERY_HISTORY_ACK;

    std::vector<Message> messages = messageModel_.queryHistory(userid, chat_id, msg_type, offset, limit);

    json msgArray = json::array();
    for (const auto& msg : messages) {
        json msgJson;
        msgJson["message_id"] = msg.id;
        msgJson["from_id"] = msg.from_id;
        msgJson["to_id"] = msg.to_id;
        msgJson["msg_type"] = msg.msg_type;
        msgJson["content"] = msg.content;
        msgJson["is_recalled"] = msg.is_recalled;
        msgJson["created_at"] = msg.created_at;
        msgArray.push_back(msgJson);
    }

    response["errno"] = 0;
    response["messages"] = msgArray;
    response["count"] = messages.size();
    LOG_INFO << "query history: user=" << userid << " chat=" << chat_id
             << " type=" << msg_type << " count=" << messages.size();

    codecSend(conn, response.dump());
}

void ChatService::messageReadAck(const TcpConnectionPtr& conn, json& js, Timestamp) {
    int userid = js["id"].get<int>();
    long long message_id = js["message_id"].get<long long>();

    if (messageModel_.markAsRead(message_id, userid)) {
        LOG_INFO << "message read ack: id=" << message_id << " user=" << userid;

        // 通知发送者消息已读（可选）
        if (js.contains("from")) {
            int fromid = js["from"].get<int>();
            json notifyMsg;
            notifyMsg["msgid"] = MESSAGE_READ_ACK;
            notifyMsg["message_id"] = message_id;
            notifyMsg["reader"] = userid;

            std::lock_guard<std::mutex> lock(connMutex_);
            auto it = userConnMap_.find(fromid);
            if (it != userConnMap_.end()) {
                codecSend(it->second, notifyMsg.dump());
            } else {
                User user = userModel_.query(fromid);
                if (user.id() != -1 && user.state() == "online") {
                    redis_.publish(fromid, notifyMsg.dump());
                }
            }
        }
    } else {
        LOG_WARN << "message read ack failed: id=" << message_id << " user=" << userid;
    }
}

// v2.0 AI 功能实现

// curl 写回调函数
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void ChatService::handleAIRequest(int groupid, int userid, const std::string& message) {
    // 异步调用 AI 服务，避免阻塞主线程
    std::thread([this, groupid, userid, message]() {
        try {
            LOG_INFO << "AI request from user " << userid << " in group " << groupid;

            // 调用 AI 服务
            std::string aiResponse = callAIService(message);

            if (aiResponse.empty()) {
                LOG_ERROR << "AI service returned empty response";
                return;
            }

            LOG_INFO << "AI response: " << aiResponse.substr(0, 50) << "...";

            // 存储 AI 回复到数据库
            long long message_id = messageModel_.insertMessage(999999, groupid, "group", aiResponse);

            // 构建 AI 回复消息
            json aiMsg;
            aiMsg["msgid"] = AI_CHAT_MSG;
            aiMsg["id"] = 999999;
            aiMsg["name"] = "AI";
            aiMsg["groupid"] = groupid;
            aiMsg["msg"] = aiResponse;
            aiMsg["time"] = std::time(nullptr);
            if (message_id > 0) {
                aiMsg["message_id"] = message_id;
            }

            // 广播 AI 回复到群组所有成员
            std::vector<int> users = groupModel_.queryGroupUsers(groupid);

            std::lock_guard<std::mutex> lock(connMutex_);
            for (int id : users) {
                if (id == 999999) continue; // 跳过 AI 自己

                auto it = userConnMap_.find(id);
                if (it != userConnMap_.end()) {
                    codecSend(it->second, aiMsg.dump());
                } else {
                    User user = userModel_.query(id);
                    if (user.id() != -1 && user.state() == "online") {
                        redis_.publish(id, aiMsg.dump());
                    } else {
                        offlineMsgModel_.insert(id, aiMsg.dump());
                    }
                }
            }

        } catch (const std::exception& e) {
            LOG_ERROR << "AI service error: " << e.what();
        }
    }).detach();
}

std::string ChatService::callAIService(const std::string& message, const std::vector<std::string>& context) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR << "curl_easy_init failed";
        return "抱歉，AI 服务暂时不可用";
    }

    // 构建请求 JSON
    json payload;
    payload["message"] = message;
    payload["context"] = json::array();

    // TODO: 添加上下文历史
    for (const auto& ctx : context) {
        payload["context"].push_back(ctx);
    }

    std::string postData = payload.dump();
    std::string response;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/api/chat");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR << "curl_easy_perform failed: " << curl_easy_strerror(res);
        return "抱歉，AI 服务连接失败";
    }

    // 解析响应
    try {
        json responseJson = json::parse(response);
        if (responseJson["success"].get<bool>()) {
            return responseJson["response"].get<std::string>();
        } else {
            std::string errorMsg = responseJson.contains("error") ? responseJson["error"].get<std::string>() : "unknown error";
            LOG_ERROR << "AI service error: " << errorMsg;
            return "抱歉，AI 处理请求时出错";
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to parse AI response: " << e.what();
        return "抱歉，AI 响应解析失败";
    }
}

