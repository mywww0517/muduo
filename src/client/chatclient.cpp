/*
以下是使用原生socket写的client(已测试可运行)
用于对比学习，好处有：
1.能看到 muduo 到底帮你省了多少事
2.面试经常问原生 socket 编程的流程
3.不依赖 mymuduo 库，更简单
*/

/*
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "protocol.hpp"
#include "json_helper.hpp"

int main()
{
    // 创建 socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // 连接服务器
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "Failed to connect" << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "Connected to ChatServer 127.0.0.1:8888" << std::endl;
    std::cout << "Commands: ping | quit" << std::endl;

    // 循环交互
    char buf[4096];
    std::string line;

    while (std::cout << ">>> " && std::getline(std::cin, line))
    {
        if (line == "quit") break;
        if (line.empty()) continue;

        std::string request;

        if (line == "ping")
        {
            request = makeMessage(PING_MSG, "ping");
        }
        else
        {
            // 其他输入暂时也包成 JSON 发过去（后续扩展用）
            request = makeMessage(0, line);
        }

        std::cout << "[Send] " << request << std::endl;

        // 发送 
        ssize_t nw = write(sockfd, request.c_str(), request.size());
        if (nw <= 0)
        {
            std::cerr << "Write error" << std::endl;
            break;
        }

        // 接收 
        memset(buf, 0, sizeof(buf));
        ssize_t nr = read(sockfd, buf, sizeof(buf) - 1);
        if (nr <= 0)
        {
            std::cout << "Server disconnected" << std::endl;
            break;
        }

        // 解析服务器返回的 JSON
        std::string response(buf, nr);
        std::cout << "[Recv] " << response << std::endl;

        try
        {
            json j = parseMessage(response);

            int msgid = j["msgid"].get<int>();

            if (msgid == PONG_MSG)
            {
                std::cout << "  → Server replied PONG ✓" << std::endl;
            }
            else if (j.contains("error"))
            {
                std::cout << "  → Server error: " << j["error"] << std::endl;
            }
        }
        catch (const json::exception &e)
        {
            std::cerr << "  → JSON parse error: " << e.what() << std::endl;
        }
    }

    close(sockfd);
    std::cout << "Disconnected" << std::endl;

    return 0;
}
*/

/*
以下是旧版代码(无codex)

#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>

#include <functional>
#include <iostream>
#include <string>

#include "protocol.hpp"
#include "json_helper.hpp"

using namespace muduo;
using namespace muduo::net;
using namespace std::placeholders;

class ChatClient
{
public:
    ChatClient(EventLoop *loop, const InetAddress &serverAddr)
        : loop_(loop),
          client_(loop, serverAddr, "ChatClient")
    {
        client_.setConnectionCallback(
            std::bind(&ChatClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&ChatClient::onMessage, this, _1, _2, _3));
    }

    void connect()
    {
        client_.connect();
    }

    // 主线程调用，把消息转发到 IO 线程发送
    // 为什么要这样做？
    //   muduo 的 TcpConnection::send() 本身是线程安全的
    //   它内部会判断：如果当前不在 IO 线程，就用 runInLoop 转发
    //   所以这里直接调 conn->send() 就行
    void send(const std::string &msg)
    {
        // conn_ 可能还没建立（connect 是异步的）
        // 用 MutexLockGuard 保护，防止主线程和 IO 线程竞争
        MutexLockGuard lock(mutex_);
        if (conn_)
        {
            conn_->send(msg);
        }
        else
        {
            std::cout << "Not connected yet" << std::endl;
        }
    }

private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO << "Connected to " << conn->peerAddress().toIpPort();

            // 保存连接，主线程的 send() 要用
            MutexLockGuard lock(mutex_);
            conn_ = conn;
        }
        else
        {
            LOG_INFO << "Disconnected from " << conn->peerAddress().toIpPort();

            MutexLockGuard lock(mutex_);
            conn_.reset();   // 清空连接

            loop_->quit();
        }
    }

    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        std::string message = buf->retrieveAllAsString();


        // 解析服务器返回的 JSON
        try
        {
            json j = parseMessage(message);
            int msgid = j["msgid"].get<int>();

            if (msgid == PONG_MSG)
            {
                std::cout << "[Recv] " << message << std::endl;
                std::cout << "  → Server replied PONG ✓" << std::endl;
            }
            else if (j.contains("error"))
            {
                std::cout << "[Recv] " << message << std::endl;
                std::cout << "  → Server error: " << j["error"] << std::endl;
            }
            else
            {
                std::cout << "[Recv] " << message << std::endl;
            }
        }
        catch (const json::exception &e)
        {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }

        // 打印提示符，让用户知道可以继续输入
        std::cout << ">>> " << std::flush;
    }

    EventLoop *loop_;
    TcpClient client_;
    TcpConnectionPtr conn_;
    MutexLock mutex_;          // 保护 conn_（主线程写，IO 线程也写）
};

int main()
{
    // 关键设计：两个线程
    //
    //   主线程：阻塞在 getline 等待用户输入
    //   IO 线程：跑 EventLoop，处理网络收发
    //
    // 为什么需要两个线程？
    //   getline 是阻塞的，会卡住当前线程
    //   如果 EventLoop 和 getline 在同一个线程
    //   要么没法读网络（卡在等输入）
    //   要么没法读输入（卡在 loop）
    //
    // EventLoopThread 就是 muduo 提供的
    // "在新线程里创建并运行一个 EventLoop" 的工具类

    EventLoopThread loopThread;
    EventLoop *loop = loopThread.startLoop();   // 在新线程启动 EventLoop

    InetAddress serverAddr("127.0.0.1", 8888);
    ChatClient client(loop, serverAddr);
    client.connect();

    std::cout << "Connecting to ChatServer 127.0.0.1:8888 ..." << std::endl;
    std::cout << "Commands: ping | quit" << std::endl;

    // 主线程：读取用户输入，构造 JSON 消息发送
    std::string line;
    while (std::cout << ">>> " && std::getline(std::cin, line))
    {
        if (line == "quit") break;
        if (line.empty()) continue;

        std::string request;

        if (line == "ping")
        {
            request = makeMessage(PING_MSG, "ping");
        }
        else
        {
            request = makeMessage(-1, line);
        }

        std::cout << "[Send] " << request << std::endl;
        client.send(request);
    }

    // 用户输入 quit 后，关闭连接
    // 不需要手动 close，TcpClient 析构时会处理
    std::cout << "Disconnected" << std::endl;

    return 0;
}
*/


#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>

#include <functional>
#include <iostream>
#include <string>

// [DAY5 新增] 主菜单循环里要等待连接、记录登录状态、做简单延时
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>

#include "protocol.hpp"

// [DAY5 修改] 不再使用 json_helper.hpp
// 原因：现在消息不再只是 msgid + data，而是会有 name/password/id 等多个字段
#include "json.hpp"

#include "codec.hpp"

// [DAY5 新增] 直接使用 nlohmann::json
using json = nlohmann::json;

using namespace muduo;
using namespace muduo::net;
using namespace std::placeholders;

class ChatClient
{
public:
    ChatClient(EventLoop *loop, const InetAddress &serverAddr)
        : loop_(loop),
          client_(loop, serverAddr, "ChatClient"),
          connected_(false),     // [DAY5 新增] 是否已建立 TCP 连接
          loggedIn_(false),      // [DAY5 新增] 是否已登录
          currentUserId_(-1)     // [DAY5 新增] 当前登录用户 id
    {
        client_.setConnectionCallback(
            std::bind(&ChatClient::onConnection, this, _1));

        // 同样用 codec 包一层
        client_.setMessageCallback(
            [this](const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
            {
                codecOnMessage(conn, buf, time,
                    std::bind(&ChatClient::onJsonMessage, this, _1, _2, _3));
            }
        );
    }

    void connect()
    {
        client_.connect();
    }

    // 主线程调用，把消息转发到 IO 线程发送
    // 为什么要这样做？
    //   muduo 的 TcpConnection::send() 本身是线程安全的
    //   它内部会判断：如果当前不在 IO 线程，就用 runInLoop 转发
    //   所以这里直接调 conn->send() 就行
    //
    // new:通过codec发送
    void send(const std::string &msg)
    {
        // conn_ 可能还没建立（connect 是异步的）
        // 用 MutexLockGuard 保护，防止主线程和 IO 线程竞争
        MutexLockGuard lock(mutex_);
        if (conn_)
        {
            codecSend(conn_, msg);
        }
        else
        {
            std::cout << "Not connected yet" << std::endl;
        }
    }

    // [DAY5 新增] 主菜单循环
    // 原来 main() 里是“自由输入 ping / quit”
    // 现在改成：
    //   未登录：register / login / quit
    //   已登录：ping / logout / quit
    void mainLoop()
    {
        // [DAY5 新增] connect() 是异步的，先等连接建立
        while (!connected_.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        while (true)
        {
            // [DAY5 新增] 如果连接已经断开，就退出客户端主循环
            if (!connected_.load())
            {
                std::cout << "连接已断开，客户端退出" << std::endl;
                break;
            }

            if (!loggedIn_.load())
            {
                showMainMenu();
            }
            else
            {
                showChatMenu();
            }

            std::string line;
            if (!std::getline(std::cin, line))
            {
                break;
            }

            if (line.empty())
            {
                continue;
            }

            if (!loggedIn_.load())
            {
                handleMainMenu(line);
            }
            else
            {
                handleChatMenu(line);
            }
        }
    }

private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO << "Connected to " << conn->peerAddress().toIpPort();

            // 保存连接，主线程的 send() 要用
            {
                MutexLockGuard lock(mutex_);
                conn_ = conn;
            }

            // [DAY5 新增] 记录连接状态
            connected_ = true;
            std::cout << "✅ 已连接到服务器" << std::endl;
        }
        else
        {
            LOG_INFO << "Disconnected from " << conn->peerAddress().toIpPort();

            {
                MutexLockGuard lock(mutex_);
                conn_.reset();   // 清空连接
            }

            // [DAY5 新增] 连接断开时，客户端状态也一起清空
            connected_ = false;
            loggedIn_ = false;
            currentUserId_ = -1;

            std::cout << "❌ 与服务器断开连接" << std::endl;
            loop_->quit();
        }
    }

    void onJsonMessage(const TcpConnectionPtr &conn, const std::string &message, Timestamp time)
    {
        // 解析服务器返回的 JSON
        try
        {
            // [DAY5 修改] 不再使用 parseMessage(message)
            // 直接 json::parse(message)
            json j = json::parse(message);
            int msgid = j["msgid"].get<int>();

            // [DAY5 新增] 根据不同 msgid 分别处理响应
            if (msgid == REG_MSG_ACK)
            {
                handleRegResponse(j);
            }
            else if (msgid == LOGIN_MSG_ACK)
            {
                handleLoginResponse(j);
            }
            else if (msgid == PONG_MSG)
            {
                std::cout << "[PONG] " << j["data"].get<std::string>() << std::endl;
            }
            else if (msgid == ERROR_MSG)
            {
                // [DAY5 新增] 兼容 error / errmsg 两种字段名
                if (j.contains("error"))
                {
                    std::cout << "[Server Error] " << j["error"].get<std::string>() << std::endl;
                }
                else if (j.contains("errmsg"))
                {
                    std::cout << "[Server Error] " << j["errmsg"].get<std::string>() << std::endl;
                }
                else
                {
                    std::cout << "[Server Error] " << j.dump() << std::endl;
                }
            }
            else
            {
                std::cout << "[Recv] " << message << std::endl;
            }
        }
        catch (const json::exception &e)
        {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }

        // [DAY5 修改] 这里不再像原来那样打印 >>> 提示符
        // 因为现在已经不是“自由输入模式”，而是菜单模式
    }

    // [DAY5 新增] 处理注册响应
    void handleRegResponse(const json &j)
    {
        int err = j["errno"].get<int>();
        if (err == 0)
        {
            std::cout << "✅ 注册成功！你的 ID = "
                      << j["id"].get<int>()
                      << "（请记住这个 ID 用于登录）" << std::endl;
        }
        else
        {
            std::cout << "❌ 注册失败："
                      << j["errmsg"].get<std::string>() << std::endl;
        }
    }

    // [DAY5 新增] 处理登录响应
    void handleLoginResponse(const json &j)
    {
        int err = j["errno"].get<int>();
        if (err == 0)
        {
            currentUserId_ = j["id"].get<int>();
            loggedIn_ = true;

            std::cout << "✅ 登录成功！欢迎 "
                      << j["name"].get<std::string>()
                      << " (id=" << currentUserId_.load() << ")" << std::endl;
        }
        else
        {
            std::cout << "❌ 登录失败："
                      << j["errmsg"].get<std::string>() << std::endl;
        }
    }

    // [DAY5 新增] 未登录菜单
    void showMainMenu()
    {
        std::cout << "\n=============================\n";
        std::cout << "  1. register  (注册)\n";
        std::cout << "  2. login     (登录)\n";
        std::cout << "  3. quit      (退出)\n";
        std::cout << "=============================\n";
        std::cout << "choice: ";
    }

    // [DAY5 新增] 登录后菜单
    void showChatMenu()
    {
        std::cout << "\n[已登录 id=" << currentUserId_.load() << "]\n";
        std::cout << "  ping    - 心跳测试\n";
        std::cout << "  logout  - 登出\n";
        std::cout << "  quit    - 退出程序\n";
        std::cout << ">>> ";
    }

    // [DAY5 新增] 处理未登录菜单
    void handleMainMenu(const std::string &choice)
    {
        if (choice == "1" || choice == "register")
        {
            doRegister();
        }
        else if (choice == "2" || choice == "login")
        {
            doLogin();
        }
        else if (choice == "3" || choice == "quit")
        {
            std::cout << "Bye!" << std::endl;
            shutdownConnection();
            std::exit(0);
        }
        else
        {
            std::cout << "无效选项" << std::endl;
        }
    }

    // [DAY5 新增] 处理登录后菜单
    void handleChatMenu(const std::string &cmd)
    {
        if (cmd == "ping")
        {
            json j;
            j["msgid"] = PING_MSG;
            j["data"] = "ping";
            send(j.dump());
        }
        else if (cmd == "logout")
        {
            json j;
            j["msgid"] = LOGOUT_MSG;
            j["id"] = currentUserId_.load();
            send(j.dump());

            // [DAY5 简化处理]
            // 这里先本地清空登录状态，不等待服务端 ACK
            // 后面如果你实现了 LOGOUT_MSG_ACK，再升级成“等 ACK 再清空”
            loggedIn_ = false;
            currentUserId_ = -1;

            std::cout << "已登出" << std::endl;
        }
        else if (cmd == "quit")
        {
            if (loggedIn_.load())
            {
                json j;
                j["msgid"] = LOGOUT_MSG;
                j["id"] = currentUserId_.load();
                send(j.dump());

                // [DAY5 新增] 稍等一下，尽量让 logout 发出去
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            shutdownConnection();
            std::exit(0);
        }
        else
        {
            std::cout << "未知命令" << std::endl;
        }
    }

    // [DAY5 新增] 注册流程：读输入 -> 组 JSON -> 发送
    void doRegister()
    {
        std::string name, pwd;
        std::cout << "用户名: ";
        std::getline(std::cin, name);
        std::cout << "密  码: ";
        std::getline(std::cin, pwd);

        json j;
        j["msgid"] = REG_MSG;
        j["name"] = name;
        j["password"] = pwd;
        send(j.dump());

        // [DAY5 简化处理]
        // 先简单 sleep 一下，避免回包和菜单提示打印在一起
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // [DAY5 新增] 登录流程：读输入 -> 组 JSON -> 发送
    void doLogin()
    {
        std::string idStr, pwd;
        std::cout << "用户 ID: ";
        std::getline(std::cin, idStr);

        int id = 0;
        try
        {
            id = std::stoi(idStr);
        }
        catch (...)
        {
            std::cout << "ID 必须是数字" << std::endl;
            return;
        }

        std::cout << "密  码: ";
        std::getline(std::cin, pwd);

        json j;
        j["msgid"] = LOGIN_MSG;
        j["id"] = id;
        j["password"] = pwd;
        send(j.dump());

        // [DAY5 简化处理] 同上
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // [DAY5 新增] 安全关闭连接
    void shutdownConnection()
    {
        MutexLockGuard lock(mutex_);
        if (conn_)
        {
            conn_->shutdown();
        }
    }

private:
    EventLoop *loop_;
    TcpClient client_;
    TcpConnectionPtr conn_;
    MutexLock mutex_;          // 保护 conn_（主线程写，IO 线程也写）

    // [DAY5 新增] 客户端状态
    std::atomic<bool> connected_;
    std::atomic<bool> loggedIn_;
    std::atomic<int> currentUserId_;
};

int main()
{
    // 关键设计：两个线程
    //
    //   主线程：阻塞在 getline 等待用户输入
    //   IO 线程：跑 EventLoop，处理网络收发
    //
    // 为什么需要两个线程？
    //   getline 是阻塞的，会卡住当前线程
    //   如果 EventLoop 和 getline 在同一个线程
    //   要么没法读网络（卡在等输入）
    //   要么没法读输入（卡在 loop）
    //
    // EventLoopThread 就是 muduo 提供的
    // "在新线程里创建并运行一个 EventLoop" 的工具类

    EventLoopThread loopThread;
    EventLoop *loop = loopThread.startLoop();   // 在新线程启动 EventLoop

    InetAddress serverAddr("127.0.0.1", 8888);
    ChatClient client(loop, serverAddr);
    client.connect();

    std::cout << "Connecting to ChatServer 127.0.0.1:8888 ..." << std::endl;

    // [DAY5 修改] 原来 main() 里自己写 while(getline)
    // 现在改成让 ChatClient 进入菜单循环
    client.mainLoop();

    // 用户退出后，程序结束
    std::cout << "Disconnected" << std::endl;
    return 0;
}
