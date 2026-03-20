#include "db.hpp"
#include <iostream>

int main() {
    MySQL mysql;
    if (!mysql.connect()) {
        std::cout << "❌ 数据库连接失败" << std::endl;
        return 1;
    }

    std::cout << "✅ 数据库连接成功" << std::endl;

    MYSQL_RES* res = mysql.query("SELECT * FROM user");
    if (res != nullptr) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            std::cout << "id=" << row[0]
                      << " name=" << row[1]
                      << " password=" << row[2]
                      << " state=" << row[3]
                      << std::endl;
        }
        mysql_free_result(res);
    }

    bool ok = mysql.update(
        "INSERT INTO user(name, password) VALUES('dbtest', '111')"
    );
    std::cout << (ok ? "✅ 插入成功" : "❌ 插入失败") << std::endl;

    res = mysql.query("SELECT * FROM user WHERE name='dbtest'");
    if (res != nullptr) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row != nullptr) {
            std::cout << "✅ 查询插入结果成功: id=" << row[0]
                      << " name=" << row[1] << std::endl;
        }
        mysql_free_result(res);
    }

    mysql.update("DELETE FROM user WHERE name='dbtest'");
    return 0;
}
