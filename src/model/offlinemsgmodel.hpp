#pragma once
#include "db.hpp"
#include <string>
#include <vector>
#include <mutex>

class OfflineMsgModel {
public:
    bool init();
    bool insert(int userid, const std::string& msg);
    std::vector<std::string> query(int userid);
    bool remove(int userid);

private:
    MySQL mysql_;
    std::mutex dbMutex_;
};
