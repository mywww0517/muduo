/*
以下是使用原生socket写的client(已测试可运行)
用于对比学习，好处有：
1.能看到 muduo 到底帮你省了多少事
2.面试经常问原生 socket 编程的流程
3.不依赖 mymuduo 库，更简单
*/

/*
#include <iostream>
#include <cstring>        //memset
#include <unistd.h>       //read,write,close
#include <arpa/inet.h>    //inet_pton,htons
#include <sys/socket.h>   //socket,connect

int main(){
    // 1.创建socket
    // AF_INET     = IPv4
    // SOCK_STREAM = TCP（面向连接的可靠传输）
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
      std::cerr << "创建socket失败";
      return 1;
    }

    //2.填写服务器地址
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // 3.连接服务器
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        std::cerr << "连接服务器失败" << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "连接到 127.0.0.1:8888" << std::endl;
    std::cout << "Type a message and press Enter (type 'quit' to exit)" << std::endl;

    // 4. 循环：输入 → 发送 → 接收回显
    char buf[1024];
    std::string line;

    while (std::cout << ">>> " && std::getline(std::cin, line))
    {
        if (line == "quit") break;
        if (line.empty()) continue;

        // 发送数据给服务器
        ssize_t nw = write(sockfd, line.c_str(), line.size());
        if (nw <= 0)
        {
            std::cerr << "写入失败" << std::endl;
            break;
        }

        // 接收服务器的回显
        memset(buf, 0, sizeof(buf));
        ssize_t nr = read(sockfd, buf, sizeof(buf) - 1);
        if (nr <= 0)
        {
            std::cout << "服务器失去连接" << std::endl;
            break;
        }

        std::cout << "<<< " << buf << std::endl;
    }

    // 关闭连接
    close(sockfd);
    std::cout << "连接关闭" << std::endl;

    return 0;
}
*/

#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Logging.h>

#include <functional>
#include <iostream>
#include <string>

using namespace muduo;
using namespace muduo::net;
using namespace std::placeholders;

class EchoClient
{
public:
    EchoClient(EventLoop *loop, const InetAddress &serverAddr)
        : loop_(loop),
          client_(loop, serverAddr, "EchoClient")
    {
        // 注册回调（和 server 端完全对称）
        client_.setConnectionCallback(
            std::bind(&EchoClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&EchoClient::onMessage, this, _1, _2, _3));
    }

    void connect()
    {
        client_.connect();   // 发起连接
    }

private:
    // 连接建立/断开 回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO << "连接到" << conn->peerAddress().toIpPort();

            // 连接建立后，发送一条测试消息
            conn->send("hello!\n");
        }
        else
        {
            LOG_INFO << "断开连接" << conn->peerAddress().toIpPort();
            loop_->quit();   // 断开后退出事件循环
        }
    }

    // 收到数据 回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
    {
        // 取出服务器回显的数据
        std::string msg = buf->retrieveAllAsString();
        LOG_INFO << "消息接收: " << msg;
    }

    EventLoop *loop_;
    TcpClient client_;
};

int main()
{
    EventLoop loop;
    InetAddress serverAddr("127.0.0.1", 8888);

    EchoClient client(&loop, serverAddr);
    client.connect();      // 发起连接
    loop.loop();  

    return 0;
}
