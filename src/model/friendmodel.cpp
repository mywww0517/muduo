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

// v1.0 好友管理增强功能实现

bool FriendModel::deleteFriend(int userid, int friendid) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "DELETE FROM friend WHERE (userid = %d AND friendid = %d) OR (userid = %d AND friendid = %d)",
        userid, friendid, friendid, userid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

bool FriendModel::setFriendRemark(int userid, int friendid, const std::string& remark) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "UPDATE friend SET remark = '%s' WHERE userid = %d AND friendid = %d",
        remark.c_str(), userid, friendid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

bool FriendModel::blockFriend(int userid, int friendid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE friend SET is_blocked = 1 WHERE userid = %d AND friendid = %d",
        userid, friendid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

bool FriendModel::unblockFriend(int userid, int friendid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "UPDATE friend SET is_blocked = 0 WHERE userid = %d AND friendid = %d",
        userid, friendid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

bool FriendModel::isBlocked(int userid, int friendid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT is_blocked FROM friend WHERE userid = %d AND friendid = %d",
        friendid, userid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row) {
            bool blocked = (std::atoi(row[0]) == 1);
            mysql_free_result(res);
            return blocked;
        }
        mysql_free_result(res);
    }
    return false;
}

std::vector<User> FriendModel::queryFriendsWithDetail(int userid) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT u.id, u.name, u.state, f.remark, f.is_blocked FROM user u "
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
            user.setRemark(row[3] ? row[3] : "");
            user.setIsBlocked(std::atoi(row[4]) == 1);
            friends.push_back(user);
        }
        mysql_free_result(res);
    }
    return friends;
}
