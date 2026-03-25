#include "usermodel.hpp"
#include "crypto.hpp"
#include <cstdio>
#include <cstdlib>

bool UserModel::init() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.connect();
}

bool UserModel::insert(User& user){
    std::string hashedPwd = sha256(user.password());

    char sql[1024] = {0};
    snprintf(sql,sizeof(sql),
        "INSERT INTO user(name, password, state) VALUES('%s', '%s', '%s')",
        user.name().c_str(), hashedPwd.c_str(), user.state().c_str());

    std::lock_guard<std::mutex> lock(dbMutex_);
    if (mysql_.update(sql)) {
        user.setId(static_cast<int>(mysql_insert_id(mysql_.getConnection())));
        return true;
    }
    return false;
}

User UserModel::query(int id) {
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql), "SELECT * FROM user WHERE id = %d", id);

    std::lock_guard<std::mutex> lock(dbMutex_);
    MYSQL_RES* res = mysql_.query(sql);
    if (res != nullptr) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row != nullptr) {
            User user;
            user.setId(std::atoi(row[0]));
            user.setName(row[1]);
            user.setPassword(row[2]);
            user.setState(row[3]);
            mysql_free_result(res);
            return user;
        }
        mysql_free_result(res);
    }

    return User();  // id = -1，表示没找到
}

bool UserModel::updateState(const User& user) {
    char sql[1024] = {0};
    snprintf(sql, sizeof(sql),
        "UPDATE user SET state = '%s' WHERE id = %d",
        user.state().c_str(), user.id());

    std::lock_guard<std::mutex> lock(dbMutex_);
    return mysql_.update(sql);
}

void UserModel::resetState() {
    std::lock_guard<std::mutex> lock(dbMutex_);
    mysql_.update("UPDATE user SET state = 'offline'");
}
