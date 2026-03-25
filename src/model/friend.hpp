#pragma once
#include "user.hpp"
#include <vector>

class Friend {
public:
    Friend(int userid = -1, int friendid = -1)
        : userid_(userid), friendid_(friendid) {}

    void setUserId(int id) { userid_ = id; }
    void setFriendId(int id) { friendid_ = id; }

    int userId() const { return userid_; }
    int friendId() const { return friendid_; }

private:
    int userid_;
    int friendid_;
};
