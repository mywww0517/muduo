#include "offlinemsgmodel.hpp"
#include <cstdio>

bool OfflineMsgModel::init() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.connect();
}

bool OfflineMsgModel::insert(int userid, const std::string& msg) {
    char sql[4096];
    snprintf(sql, sizeof(sql),
        "INSERT INTO offlinemessage(userid, message) VALUES(%d, '%s')",
        userid, msg.c_str());

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

std::vector<std::string> OfflineMsgModel::query(int userid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT message FROM offlinemessage WHERE userid = %d", userid);

    std::vector<std::string> msgs;
    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            msgs.push_back(row[0]);
        }
        mysql_free_result(res);
    }
    return msgs;
}

bool OfflineMsgModel::remove(int userid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE FROM offlinemessage WHERE userid = %d", userid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}
