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

#include "protocol.hpp"
#include "json_helper.hpp"
#include "codec.hpp"

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

    void onJsonMessage(const TcpConnectionPtr &conn, const std::string &message, Timestamp time)
    {
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