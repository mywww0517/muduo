#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

#include "codec.hpp"
#include "chatservice.hpp"
#include "json.hpp"

using json = nlohmann::json;
using namespace muduo;
using namespace muduo::net;
using namespace std::placeholders;

class ChatServer{
public:
    ChatServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        :server_(loop, addr, name){
            server_.setConnectionCallback(
                std::bind(&ChatServer::onConnection, this, _1)
            );

            // 改动：不再直接注册onMessage
            // 而是注册codec的解码函数
            // codec 解码成功后再调onJsonMessage
            server_.setMessageCallback(
                [this](const TcpConnectionPtr &conn, Buffer *buf, Timestamp time){
                    codecOnMessage(conn,buf,time,
                        std::bind(&ChatServer::onJsonMessage, this, _1, _2, _3));
                }
            );
            server_.setThreadNum(4);
    }

    void start(){
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr &conn){
        if(conn->connected()){
            LOG_INFO << "新的连接：" << conn->peerAddress().toIpPort();
        }
        else{
            LOG_INFO << "连接关闭：" << conn->peerAddress().toIpPort();
            ChatService::instance().clientCloseException(conn);
        }
    }

/*  以下是旧版函数（没使用codec）

    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time){
        std::string received = buf->retrieveAllAsString();
        LOG_INFO << "收到消息: " << received;

        //解析JSON
        try{
            json request = parseMessage(received);

            int msgid = request["msgid"].get<int>();

            switch(msgid){
            case PING_MSG:{
                LOG_INFO << "Got PING from: " << conn->peerAddress().toIpPort();

                std::string response = makeMessage(PONG_MSG,"pong");
                conn->send(response);
                LOG_INFO << "Send PONG to: " << response;
                break;
            }
            default: {
                LOG_INFO << "Unknown msgid: " << msgid;

                json errResp;
                errResp["msgid"] = -1;
                errResp["data"] = "Unknown msgid";
                conn->send(errResp.dump());
                break;
            }
            }
        }
        catch (const json::exception &e){
            LOG_ERROR << "JSON解析失败：" << e.what();

            json errResp;
            errResp["msgid"] = -1;
            errResp["error"] = "invaild json";
            conn->send(errResp.dump());
        }
    } 
*/

     void onJsonMessage(const TcpConnectionPtr &conn, const std::string &message, Timestamp time){
        //改动：不再直接从buf读取消息
        LOG_INFO << "收到消息: " << message;

        try{
            json js = json::parse(message);

            int msgid = js["msgid"].get<int>();

            // 根据 msgid 分发到对应 handler
            auto handler = ChatService::instance().getHandler(msgid);
            handler(conn, js, time);

            /*
            switch(msgid){
            case PING_MSG:{
                LOG_INFO << "Got PING from: " << conn->peerAddress().toIpPort();

                std::string response = makeMessage(PONG_MSG,"pong");
                codecSend(conn,response);  //改动：用codecsend
                LOG_INFO << "Send PONG to: " << response;
                break;
            }
            default: {
                LOG_INFO << "Unknown msgid: " << msgid;

                json errResp;
                errResp["msgid"] = -1;
                errResp["data"] = "Unknown msgid";
                conn->send(errResp.dump());
                break;
            }
            */
        }
        catch (const json::exception &e){
            LOG_ERROR << "JSON解析失败：" << e.what();
            json response;
            response["msgid"] = 0;
            response["error"] = "invaild json";
            conn->send(response.dump());
        }
    } 

    TcpServer server_;
};

int main(){
    // 初始化业务层（连接数据库等）
    if (!ChatService::instance().init()) {
        LOG_FATAL << "ChatService init failed";
        return 1;
    }

    // 重置所有用户状态
    ChatService::instance().reset();

    EventLoop loop;
    InetAddress addr(8888);

    ChatServer server(&loop, addr, "ChatServer");
    server.start();

    LOG_INFO << "ChatServer running on port 8888";

    loop.loop();

    return 0;
}