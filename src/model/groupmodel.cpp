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

// v1.0 群组管理增强功能实现

bool GroupModel::leaveGroup(int userid, int groupid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE FROM groupuser WHERE groupid = %d AND userid = %d",
        groupid, userid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

bool GroupModel::kickMember(int groupid, int operatorid, int targetid) {
    // 验证操作者是否为群主
    std::string role = getUserRole(groupid, operatorid);
    if (role != "creator") {
        return false;
    }

    // 不能踢出群主自己
    if (operatorid == targetid) {
        return false;
    }

    // 删除目标用户
    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE FROM groupuser WHERE groupid = %d AND userid = %d",
        groupid, targetid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

bool GroupModel::transferGroup(int groupid, int oldcreator, int newcreator) {
    // 验证旧群主身份
    std::string oldRole = getUserRole(groupid, oldcreator);
    if (oldRole != "creator") {
        return false;
    }

    // 验证新群主是否为群成员
    std::string newRole = getUserRole(groupid, newcreator);
    if (newRole.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(dbMutex_);

    // 将旧群主改为普通成员
    char sql1[256];
    snprintf(sql1, sizeof(sql1),
        "UPDATE groupuser SET grouprole = 'normal' WHERE groupid = %d AND userid = %d",
        groupid, oldcreator);
    if (!mysql_.update(sql1)) {
        return false;
    }

    // 将新群主设置为 creator
    char sql2[256];
    snprintf(sql2, sizeof(sql2),
        "UPDATE groupuser SET grouprole = 'creator' WHERE groupid = %d AND userid = %d",
        groupid, newcreator);
    return mysql_.update(sql2);
}

bool GroupModel::setAnnouncement(int groupid, const std::string& announcement) {
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "UPDATE allgroup SET announcement = '%s' WHERE id = %d",
        announcement.c_str(), groupid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

std::string GroupModel::getAnnouncement(int groupid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT announcement FROM allgroup WHERE id = %d", groupid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0]) {
            std::string announcement = row[0];
            mysql_free_result(res);
            return announcement;
        }
        mysql_free_result(res);
    }
    return "";
}

std::string GroupModel::getUserRole(int groupid, int userid) {
    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT grouprole FROM groupuser WHERE groupid = %d AND userid = %d",
        groupid, userid);

    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0]) {
            std::string role = row[0];
            mysql_free_result(res);
            return role;
        }
        mysql_free_result(res);
    }
    return "";
}

std::vector<GroupUser> GroupModel::queryGroupUsersWithDetail(int groupid) {
    char sql[512];
    snprintf(sql, sizeof(sql),
        "SELECT userid, grouprole FROM groupuser WHERE groupid = %d",
        groupid);

    std::vector<GroupUser> users;
    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            GroupUser gu;
            gu.setGroupId(groupid);
            gu.setUserId(std::atoi(row[0]));
            gu.setRole(row[1] ? row[1] : "normal");
            users.push_back(gu);
        }
        mysql_free_result(res);
    }
    return users;
}
