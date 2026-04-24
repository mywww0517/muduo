#pragma once
#include "group.hpp"
#include "user.hpp"
#include "db.hpp"
#include <vector>
#include <mutex>

struct GroupInfo {
    Group group;
    std::vector<User> users;
};

class GroupModel {
public:
    bool init();
    bool createGroup(Group& group);
    bool addToGroup(int userid, int groupid, const std::string& role);
    std::vector<GroupInfo> queryGroups(int userid);
    std::vector<int> queryGroupUsers(int groupid);

    // v1.0 群组管理增强
    bool leaveGroup(int userid, int groupid);
    bool kickMember(int groupid, int operatorid, int targetid);
    bool transferGroup(int groupid, int oldcreator, int newcreator);
    bool setAnnouncement(int groupid, const std::string& announcement);
    std::string getAnnouncement(int groupid);
    std::string getUserRole(int groupid, int userid);
    std::vector<GroupUser> queryGroupUsersWithDetail(int groupid);

private:
    MySQL mysql_;
    std::mutex dbMutex_;
};
