#pragma once
#include "db.hpp"
#include <vector>
#include <string>
#include <mutex>

struct Message {
    long long id;
    int from_id;
    int to_id;
    std::string msg_type;  // private / group
    std::string content;
    bool is_recalled;
    std::string created_at;
};

class MessageModel {
public:
    bool init();

    // 插入消息
    long long insertMessage(int from_id, int to_id, const std::string& msg_type, const std::string& content);

    // 撤回消息（2分钟内）
    bool recallMessage(long long message_id, int user_id);

    // 查询消息历史（分页）
    std::vector<Message> queryHistory(int user_id, int chat_id, const std::string& msg_type, int offset, int limit);

    // 标记消息已读
    bool markAsRead(long long message_id, int user_id);

    // 查询未读消息数
    int getUnreadCount(int user_id, int from_id);

private:
    MySQL mysql_;
    std::mutex dbMutex_;
};
