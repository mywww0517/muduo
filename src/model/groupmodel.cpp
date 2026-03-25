#include "groupmodel.hpp"
#include <cstdio>

bool GroupModel::init() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.connect();
}

bool GroupModel::createGroup(Group& group) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO allgroup(groupname, groupdesc) VALUES('%s', '%s')",
        group.name().c_str(), group.desc().c_str());

    std::lock_guard<std::mutex> lock(dbMutex_);
    if (mysql_.update(sql)) {
        group.setId(static_cast<int>(mysql_insert_id(mysql_.getConnection())));
        return true;
    }
    return false;
}

bool GroupModel::addToGroup(int userid, int groupid, const std::string& role) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "INSERT INTO groupuser(groupid, userid, grouprole) VALUES(%d, %d, '%s')",
        groupid, userid, role.c_str());

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

std::vector<GroupInfo> GroupModel::queryGroups(int userid) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT g.id, g.groupname, g.groupdesc FROM allgroup g "
        "INNER JOIN groupuser gu ON gu.groupid = g.id WHERE gu.userid = %d", userid);

    std::vector<GroupInfo> groups;
    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            GroupInfo info;
            info.group.setId(std::atoi(row[0]));
            info.group.setName(row[1]);
            info.group.setDesc(row[2] ? row[2] : "");

            char sql2[512];
            snprintf(sql2, sizeof(sql2),
                "SELECT u.id, u.name, u.state FROM user u "
                "INNER JOIN groupuser gu ON gu.userid = u.id WHERE gu.groupid = %d",
                info.group.id());

            MYSQL_RES* res2 = mysql_.query(sql2);
            if (res2) {
                MYSQL_ROW row2;
                while ((row2 = mysql_fetch_row(res2))) {
                    User user;
                    user.setId(std::atoi(row2[0]));
                    user.setName(row2[1]);
                    user.setState(row2[2]);
                    info.users.push_back(user);
                }
                mysql_free_result(res2);
            }
            groups.push_back(info);
        }
        mysql_free_result(res);
    }
    return groups;
}

std::vector<int> GroupModel::queryGroupUsers(int groupid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT userid FROM groupuser WHERE groupid = %d", groupid);

    std::vector<int> users;
    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            users.push_back(std::atoi(row[0]));
        }
        mysql_free_result(res);
    }
    return users;
}
