#include "usermodel.hpp"
#include <ctime>
#include <iostream>

int main() {
    UserModel model;
    if (!model.init()) {
        std::cout << "❌ 数据库连接失败" << std::endl;
        return 1;
    }
    std::cout << "✅ 数据库连接成功" << std::endl;

    User newUser;
    newUser.setName("alice_" + std::to_string(std::time(nullptr)));
    newUser.setPassword("abc123");

    if (model.insert(newUser)) {
        std::cout << "✅ 注册成功，id=" << newUser.id()
                  << " name=" << newUser.name() << std::endl;
    } else {
        std::cout << "❌ 注册失败" << std::endl;
        return 1;
    }

    User found = model.query(newUser.id());
    if (found.id() != -1) {
        std::cout << "✅ 查询成功：id=" << found.id()
                  << " name=" << found.name()
                  << " state=" << found.state() << std::endl;
    } else {
        std::cout << "❌ 查询失败" << std::endl;
        return 1;
    }

    found.setState("online");
    if (model.updateState(found)) {
        std::cout << "✅ 状态更新为 online" << std::endl;
    } else {
        std::cout << "❌ 状态更新失败" << std::endl;
        return 1;
    }

    User check = model.query(found.id());
    std::cout << "验证 state=" << check.state() << std::endl;

    model.resetState();
    check = model.query(found.id());
    std::cout << "✅ 重置后 state=" << check.state() << std::endl;

    return 0;
}
