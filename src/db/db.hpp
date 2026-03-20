#pragma once
#include <mysql/mysql.h>
#include <string>

class MySQL {
public:
    MySQL();
    ~MySQL();

    bool connect();                            // 连接数据库
    bool update(const std::string& sql);       // INSERT / UPDATE / DELETE
    MYSQL_RES* query(const std::string& sql);  // SELECT，调用方负责 mysql_free_result
    MYSQL* getConnection();                    // 获取原始连接（用于 mysql_insert_id 等）

private:
    MYSQL* conn_;
};