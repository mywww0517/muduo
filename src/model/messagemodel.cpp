#include "messagemodel.hpp"
#include <muduo/base/Logging.h>
#include <cstdio>
#include <ctime>

bool MessageModel::init() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.connect();
}

long long MessageModel::insertMessage(int from_id, int to_id, const std::string& msg_type, const std::string& content) {
    char sql[2048];

    // 转义消息内容，防止 SQL 注入
    char escaped_content[1024];
    mysql_real_escape_string(mysql_.getConnection(), escaped_content, content.c_str(), content.length());

    snprintf(sql, sizeof(sql),
        "INSERT INTO message(from_id, to_id, msg_type, content) VALUES(%d, %d, '%s', '%s')",
        from_id, to_id, msg_type.c_str(), escaped_content);

    std::lock_guard<std::mutex> lock(dbMutex_);
    if (mysql_.update(sql)) {
        // 获取插入的消息 ID
        MYSQL_RES* res = mysql_.query("SELECT LAST_INSERT_ID()");
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            long long message_id = std::atoll(row[0]);
            mysql_free_result(res);
            LOG_INFO << "Message inserted: id=" << message_id << ", from=" << from_id << ", to=" << to_id;
            return message_id;
        }
    }

    LOG_ERROR << "Failed to insert message: from=" << from_id << ", to=" << to_id;
    return -1;
}

bool MessageModel::recallMessage(long long message_id, int user_id) {
    char sql[512];

    // 检查消息是否存在、是否是发送者本人、是否在2分钟内
    snprintf(sql, sizeof(sql),
        "UPDATE message SET is_recalled = 1 "
        "WHERE id = %lld AND from_id = %d AND is_recalled = 0 "
        "AND TIMESTAMPDIFF(SECOND, created_at, NOW()) <= 120",
        message_id, user_id);

    std::lock_guard<std::mutex> lock(dbMutex_);
    bool success = mysql_.update(sql);

    if (success) {
        LOG_INFO << "Message recalled: id=" << message_id << ", user=" << user_id;
    } else {
        LOG_WARN << "Failed to recall message: id=" << message_id << ", user=" << user_id
                 << " (超过2分钟或非本人消息)";
    }

    return success;
}

std::vector<Message> MessageModel::queryHistory(int user_id, int chat_id, const std::string& msg_type, int offset, int limit) {
    char sql[1024];
    std::vector<Message> messages;

    if (msg_type == "private") {
        // 一对一聊天：查询双方的消息
        snprintf(sql, sizeof(sql),
            "SELECT id, from_id, to_id, msg_type, content, is_recalled, created_at "
            "FROM message "
            "WHERE msg_type = 'private' AND ((from_id = %d AND to_id = %d) OR (from_id = %d AND to_id = %d)) "
            "ORDER BY created_at DESC LIMIT %d OFFSET %d",
            user_id, chat_id, chat_id, user_id, limit, offset);
    } else if (msg_type == "group") {
        // 群聊：查询群组的消息
        snprintf(sql, sizeof(sql),
            "SELECT id, from_id, to_id, msg_type, content, is_recalled, created_at "
            "FROM message "
            "WHERE msg_type = 'group' AND to_id = %d "
            "ORDER BY created_at DESC LIMIT %d OFFSET %d",
            chat_id, limit, offset);
    } else {
        LOG_ERROR << "Invalid msg_type: " << msg_type;
        return messages;
    }

    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            Message msg;
            msg.id = std::atoll(row[0]);
            msg.from_id = std::atoi(row[1]);
            msg.to_id = std::atoi(row[2]);
            msg.msg_type = row[3];
            msg.content = row[4];
            msg.is_recalled = std::atoi(row[5]);
            msg.created_at = row[6];
            messages.push_back(msg);
        }
        mysql_free_result(res);
        LOG_INFO << "Query history: user=" << user_id << ", chat=" << chat_id
                 << ", type=" << msg_type << ", count=" << messages.size();
    }

    return messages;
}

bool MessageModel::markAsRead(long long message_id, int user_id) {
    char sql[512];

    // 插入或更新已读状态
    snprintf(sql, sizeof(sql),
        "INSERT INTO message_read_status(message_id, user_id, is_read, read_at) "
        "VALUES(%lld, %d, 1, NOW()) "
        "ON DUPLICATE KEY UPDATE is_read = 1, read_at = NOW()",
        message_id, user_id);

    std::lock_guard<std::mutex> lock(dbMutex_);
    bool success = mysql_.update(sql);

    if (success) {
        LOG_INFO << "Message marked as read: id=" << message_id << ", user=" << user_id;
    }

    return success;
}

int MessageModel::getUnreadCount(int user_id, int from_id) {
    char sql[512];

    // 查询未读消息数：发送给 user_id 的私聊消息，且未在 message_read_status 中标记为已读
    snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM message m "
        "LEFT JOIN message_read_status mrs ON m.id = mrs.message_id AND mrs.user_id = %d "
        "WHERE m.to_id = %d AND m.from_id = %d AND m.msg_type = 'private' "
        "AND (mrs.is_read IS NULL OR mrs.is_read = 0)",
        user_id, user_id, from_id);

    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    int count = 0;

    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row) {
            count = std::atoi(row[0]);
        }
        mysql_free_result(res);
    }

    return count;
}
