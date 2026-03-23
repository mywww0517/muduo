#include "db.hpp"
#include <muduo/base/Logging.h>

#include <cstdlib>
#include <cstring>
#include <string>

static std::string getEnv(const char* key, const char* defaultValue = "") {
    const char* value = std::getenv(key);
    if (value == nullptr || std::strlen(value) == 0) {
        return defaultValue;
    }
    return value;
}

static unsigned int getEnvUInt(const char* key, unsigned int defaultValue) {
    const char* value = std::getenv(key);
    if (value == nullptr || std::strlen(value) == 0) {
        return defaultValue;
    }
    return static_cast<unsigned int>(std::strtoul(value, nullptr, 10));
}

MySQL::MySQL() {
    conn_ = mysql_init(nullptr);
}

MySQL::~MySQL() {
    if (conn_) {
        mysql_close(conn_);
    }
}

bool MySQL::connect() {
    if (conn_ == nullptr) {
        LOG_ERROR << "mysql_init failed";
        return false;
    }

    std::string host   = getEnv("CHAT_DB_HOST", "127.0.0.1");
    std::string user   = getEnv("CHAT_DB_USER", "");
    std::string passwd = getEnv("CHAT_DB_PASSWORD", "");
    std::string dbname = getEnv("CHAT_DB_NAME", "chat");
    unsigned int port  = getEnvUInt("CHAT_DB_PORT", 3306);

    if (user.empty()) {
        LOG_ERROR << "CHAT_DB_USER is not set";
        return false;
    }

    MYSQL* p = mysql_real_connect(
        conn_,
        host.c_str(),
        user.c_str(),
        passwd.c_str(),
        dbname.c_str(),
        port,
        nullptr,
        0
    );

    if (p != nullptr) {
        mysql_query(conn_, "SET NAMES utf8mb4");
        LOG_INFO << "MySQL connect success";
        return true;
    } else {
        LOG_ERROR << "MySQL connect failed: " << mysql_error(conn_);
        return false;
    }
}

bool MySQL::update(const std::string& sql) {
    if (mysql_query(conn_, sql.c_str())) {
        LOG_ERROR << "MySQL update failed: " << sql
                  << " error: " << mysql_error(conn_);
        return false;
    }
    return true;
}

MYSQL_RES* MySQL::query(const std::string& sql) {
    if (mysql_query(conn_, sql.c_str())) {
        LOG_ERROR << "MySQL query failed: " << sql
                  << " error: " << mysql_error(conn_);
        return nullptr;
    }
    return mysql_store_result(conn_);
}

MYSQL* MySQL::getConnection() {
    return conn_;
}
