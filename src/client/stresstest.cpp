// 粘包压力测试
// 快速连续发送 N 条消息，验证 server 能正确拆包处理每一条
// 如果没有 codec，这里大概率会粘包导致 JSON 解析失败

#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/base/Logging.h>
#include <muduo/base/CountDownLatch.h>

#include <iostream>
#include <atomic>

#include "protocol.hpp"
#include "json_helper.hpp"
#include "codec.hpp"

using namespace muduo;
using namespace muduo::net;
using namespace std::placeholders;

class StressClient
{
public:
    StressClient(EventLoop *loop, const InetAddress &addr, int count)
        : client_(loop, addr, "StressClient"),
          totalSend_(count),
          recvCount_(0),
          latch_(1)   // 等待连接建立
    {
        client_.setConnectionCallback(
            std::bind(&StressClient::onConnection, this, _1));
        client_.setMessageCallback(
            [this](const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
            {
                codecOnMessage(conn, buf, time,
                    std::bind(&StressClient::onJsonMessage, this, _1, _2, _3));
            }
        );
    }

    void connect() { client_.connect(); }

    // 阻塞等待连接建立
    void waitConnected() { latch_.wait(); }

    void sendAll()
    {
        MutexLockGuard lock(mutex_);
        for (int i = 0; i < totalSend_; ++i)
        {
            std::string msg = makeMessage(PING_MSG, "stress-" + std::to_string(i));
            codecSend(conn_, msg);
        }
        std::cout << "Sent " << totalSend_ << " messages" << std::endl;
    }

    int recvCount() const { return recvCount_.load(); }

private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            MutexLockGuard lock(mutex_);
            conn_ = conn;
            latch_.countDown();  // 通知主线程连接已建立
        }
    }

    void onJsonMessage(const TcpConnectionPtr &conn,
                       const std::string &message,
                       Timestamp time)
    {
        int n = recvCount_.fetch_add(1) + 1;
        if (n % 100 == 0 || n == totalSend_)
        {
            std::cout << "Received " << n << "/" << totalSend_ << std::endl;
        }
    }

    TcpClient client_;
    TcpConnectionPtr conn_;
    MutexLock mutex_;
    int totalSend_;
    std::atomic<int> recvCount_;
    CountDownLatch latch_;
};

int main(int argc, char *argv[])
{
    int count = 1000;  // 默认发 1000 条
    if (argc > 1) count = atoi(argv[1]);

    std::cout << "=== Stress Test: " << count << " messages ===" << std::endl;

    EventLoopThread loopThread;
    EventLoop *loop = loopThread.startLoop();

    InetAddress serverAddr("127.0.0.1", 8888);
    StressClient client(loop, serverAddr, count);
    client.connect();
    client.waitConnected();

    Timestamp start = Timestamp::now();

    client.sendAll();

    // 等待所有回复（最多等 10 秒）
    for (int i = 0; i < 100; ++i)
    {
        if (client.recvCount() >= count) break;
        usleep(100000);  // 100ms
    }

    Timestamp end = Timestamp::now();

    double elapsed = timeDifference(end, start);

    std::cout << "\n=== Result ===" << std::endl;
    std::cout << "Sent:     " << count << std::endl;
    std::cout << "Received: " << client.recvCount() << std::endl;
    std::cout << "Time:     " << elapsed << "s" << std::endl;
    std::cout << "QPS:      " << static_cast<int>(count / elapsed) << std::endl;

    if (client.recvCount() == count)
        std::cout << "✅ ALL PASSED — no packet loss!" << std::endl;
    else
        std::cout << "❌ FAILED — lost " << (count - client.recvCount()) << " messages" << std::endl;

    return 0;
}
