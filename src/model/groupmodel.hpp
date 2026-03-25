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

private:
    MySQL mysql_;
    std::mutex dbMutex_;
};
