#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

#include <functional>
#include <string>

using namespace muduo;
using namespace muduo::net;

/*
  EchoServer类

  功能：接收客户端连接，收到什么回什么(echo)
*/
class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : server_(loop,addr,name)
        ,loop_(loop){
        // 注册连接回调
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
        );

        //注册消息回调
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      std::placeholders::_3
                      )
        );

        // 设置 subloop 线程数 = 2
        // muduo 的线程模型：1 个 mainLoop 负责 accept 新连接，
        // N 个 subLoop 负责处理已建立连接的 IO（读写）
        // 这里设 2 表示有 2 个工作线程处理 IO
        // 设 0 则所有事情都在主线程完成（单线程模式）
        server_.setThreadNum(2);
    }

    void start(){
        server_.start(); //启动监听
    }

private:
    // 连接回调
    void onConnection(const TcpConnectionPtr &conn){
        if(conn->connected()){
            LOG_INFO << "连接建立：" << conn->peerAddress().toIpPort();
        }
        else{
            LOG_INFO << "连接断开：" << conn->peerAddress().toIpPort();
        }
    }

    //消息回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time){
        //将buffer中数据全取出来变成string
        std::string msg = buf->retrieveAllAsString(); 

        LOG_INFO << "消息接收：" << msg << "在" << time.toString();

        conn->send(msg);
    }

    TcpServer server_;
    EventLoop* loop_;
};

int main(){
    // EventLoop 是 muduo 的核心：事件循环
    // 可以理解为一个"永不停止的 while 循环"，不断检查：
    //   有新连接吗？有数据到达吗？有定时器到期吗？
    // 有的话就调用你注册的回调函数
    EventLoop loop;

    // InetAddress 封装了 IP + 端口
    // 只传端口号 = 监听所有网卡（0.0.0.0）的 8888 端口
    InetAddress addr(8888);

    EchoServer server(&loop, addr, "EchoServer");

    server.start();
    loop.loop();

    return 0;
}