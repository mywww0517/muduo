#include "messagemodel.hpp"
#include <iostream>
#include <thread>
#include <chrono>

void testMessageModel() {
    MessageModel messageModel;

    std::cout << "=== MessageModel 功能测试 ===" << std::endl;

    // 1. 初始化
    std::cout << "\n[测试 1] 初始化数据库连接..." << std::endl;
    if (!messageModel.init()) {
        std::cerr << "初始化失败！" << std::endl;
        return;
    }
    std::cout << "✓ 初始化成功" << std::endl;

    // 2. 插入私聊消息
    std::cout << "\n[测试 2] 插入私聊消息..." << std::endl;
    long long msg_id_1 = messageModel.insertMessage(1, 2, "private", "Hello from user1 to user2");
    if (msg_id_1 > 0) {
        std::cout << "✓ 插入成功，message_id = " << msg_id_1 << std::endl;
    } else {
        std::cerr << "✗ 插入失败" << std::endl;
    }

    // 3. 插入群聊消息
    std::cout << "\n[测试 3] 插入群聊消息..." << std::endl;
    long long msg_id_2 = messageModel.insertMessage(1, 100, "group", "Hello everyone in group 100");
    if (msg_id_2 > 0) {
        std::cout << "✓ 插入成功，message_id = " << msg_id_2 << std::endl;
    } else {
        std::cerr << "✗ 插入失败" << std::endl;
    }

    // 4. 查询私聊历史
    std::cout << "\n[测试 4] 查询私聊历史（user1 和 user2）..." << std::endl;
    std::vector<Message> private_msgs = messageModel.queryHistory(1, 2, "private", 0, 10);
    std::cout << "✓ 查询到 " << private_msgs.size() << " 条消息" << std::endl;
    for (const auto& msg : private_msgs) {
        std::cout << "  - [" << msg.id << "] from=" << msg.from_id
                  << " to=" << msg.to_id << " content=\"" << msg.content
                  << "\" recalled=" << msg.is_recalled
                  << " time=" << msg.created_at << std::endl;
    }

    // 5. 查询群聊历史
    std::cout << "\n[测试 5] 查询群聊历史（group 100）..." << std::endl;
    std::vector<Message> group_msgs = messageModel.queryHistory(1, 100, "group", 0, 10);
    std::cout << "✓ 查询到 " << group_msgs.size() << " 条消息" << std::endl;
    for (const auto& msg : group_msgs) {
        std::cout << "  - [" << msg.id << "] from=" << msg.from_id
                  << " to_group=" << msg.to_id << " content=\"" << msg.content
                  << "\" recalled=" << msg.is_recalled
                  << " time=" << msg.created_at << std::endl;
    }

    // 6. 标记消息已读
    std::cout << "\n[测试 6] 标记消息已读..." << std::endl;
    if (msg_id_1 > 0) {
        if (messageModel.markAsRead(msg_id_1, 2)) {
            std::cout << "✓ 标记成功，message_id = " << msg_id_1 << " 已被 user2 阅读" << std::endl;
        } else {
            std::cerr << "✗ 标记失败" << std::endl;
        }
    }

    // 7. 查询未读消息数
    std::cout << "\n[测试 7] 查询未读消息数..." << std::endl;
    int unread_count = messageModel.getUnreadCount(2, 1);
    std::cout << "✓ user2 有 " << unread_count << " 条来自 user1 的未读消息" << std::endl;

    // 8. 测试消息撤回（立即撤回）
    std::cout << "\n[测试 8] 测试消息撤回（立即撤回）..." << std::endl;
    long long msg_id_3 = messageModel.insertMessage(1, 2, "private", "This message will be recalled");
    if (msg_id_3 > 0) {
        std::cout << "✓ 插入消息，message_id = " << msg_id_3 << std::endl;

        if (messageModel.recallMessage(msg_id_3, 1)) {
            std::cout << "✓ 撤回成功" << std::endl;
        } else {
            std::cerr << "✗ 撤回失败" << std::endl;
        }

        // 验证撤回状态
        std::vector<Message> recalled_msgs = messageModel.queryHistory(1, 2, "private", 0, 1);
        if (!recalled_msgs.empty() && recalled_msgs[0].id == msg_id_3) {
            std::cout << "✓ 验证撤回状态：is_recalled = " << recalled_msgs[0].is_recalled << std::endl;
        }
    }

    // 9. 测试撤回权限（非本人消息）
    std::cout << "\n[测试 9] 测试撤回权限（非本人消息）..." << std::endl;
    if (msg_id_1 > 0) {
        if (messageModel.recallMessage(msg_id_1, 999)) {
            std::cerr << "✗ 不应该允许撤回他人消息" << std::endl;
        } else {
            std::cout << "✓ 正确拒绝了非本人的撤回请求" << std::endl;
        }
    }

    // 10. 测试撤回时间限制（需要等待 2 分钟，这里只是演示）
    std::cout << "\n[测试 10] 测试撤回时间限制..." << std::endl;
    std::cout << "提示：完整测试需要等待 2 分钟，这里跳过" << std::endl;
    std::cout << "如需测试，请插入消息后等待 121 秒再尝试撤回" << std::endl;

    std::cout << "\n=== 所有测试完成 ===" << std::endl;
}

int main() {
    try {
        testMessageModel();
    } catch (const std::exception& e) {
        std::cerr << "测试异常: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
