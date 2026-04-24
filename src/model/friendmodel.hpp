#pragma once
#include "user.hpp"
#include "db.hpp"
#include <vector>
#include <mutex>
#include <string>

class FriendModel {
public:
    bool init();
    bool insert(int userid, int friendid);
    std::vector<User> query(int userid);

    // v1.0 好友管理增强
    bool deleteFriend(int userid, int friendid);
    bool setFriendRemark(int userid, int friendid, const std::string& remark);
    bool blockFriend(int userid, int friendid);
    bool unblockFriend(int userid, int friendid);
    bool isBlocked(int userid, int friendid);
    std::vector<User> queryFriendsWithDetail(int userid);

private:
    MySQL mysql_;
    std::mutex dbMutex_;
};
