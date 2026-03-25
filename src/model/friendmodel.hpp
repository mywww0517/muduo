#pragma once
#include "user.hpp"
#include "db.hpp"
#include <vector>
#include <mutex>

class FriendModel {
public:
    bool init();
    bool insert(int userid, int friendid);
    std::vector<User> query(int userid);

private:
    MySQL mysql_;
    std::mutex dbMutex_;
};
