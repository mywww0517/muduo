#include "friendmodel.hpp"
#include <cstdio>

bool FriendModel::init() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.connect();
}

bool FriendModel::insert(int userid, int friendid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "INSERT INTO friend(userid, friendid) VALUES(%d, %d)", userid, friendid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

std::vector<User> FriendModel::query(int userid) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT u.id, u.name, u.state FROM user u "
        "INNER JOIN friend f ON f.friendid = u.id WHERE f.userid = %d", userid);

    std::vector<User> friends;
    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            User user;
            user.setId(std::atoi(row[0]));
            user.setName(row[1]);
            user.setState(row[2]);
            friends.push_back(user);
        }
        mysql_free_result(res);
    }
    return friends;
}
