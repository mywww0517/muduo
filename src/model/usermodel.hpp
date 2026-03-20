#pragma once
#include "user.hpp"
#include "db.hpp"
#include <mutex>

class UserModel {
public:
    bool init();

    bool insert(User& user);
    User query(int id);
    bool updateState(const User& user);
    void resetState();
    
private:
    MySQL mysql_;
    std::mutex dbMutex_;
};