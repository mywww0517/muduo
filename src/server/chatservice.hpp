#pragma once

#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>
#include <unordered_map>
#include <functional>
#include <mutex>

#include "protocol.hpp"
#include "codec.hpp"
#include "usermodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "offlinemsgmodel.hpp"
#include "messagemodel.hpp"
#include "redis.hpp"
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

    void addFriend(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void oneChat(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void createGroup(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void joinGroup(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void groupChat(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);

    // v1.0 好友管理增强
    void deleteFriend(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void setFriendRemark(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void blockFriend(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void unblockFriend(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);

    // v1.0 群组管理增强
    void leaveGroup(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void kickGroupMember(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void transferGroup(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void setGroupAnnouncement(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void getGroupMembers(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);

    // v1.0 消息功能增强
    void recallMessage(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void queryHistory(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);
    void messageReadAck(const muduo::net::TcpConnectionPtr& conn, json& js, muduo::Timestamp time);

    void clientCloseException(const muduo::net::TcpConnectionPtr& conn);
    void reset();

    void handleRedisSubscribeMessage(int userid, const std::string& msg);

private:
    ChatService();

    void broadcastGroupNotification(int groupid, const std::string& notification);

    std::unordered_map<int, MsgHandler> msgHandlerMap_;
    std::unordered_map<int, muduo::net::TcpConnectionPtr> userConnMap_;
    std::mutex connMutex_;

    UserModel userModel_;
    FriendModel friendModel_;
    GroupModel groupModel_;
    OfflineMsgModel offlineMsgModel_;
    MessageModel messageModel_;
    Redis redis_;
};
