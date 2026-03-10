#ifndef CODEC_HPP
#define CODEC_HPP

// 编解码器（codec = coder + decoder）
//
// 解决 TCP 粘包/半包问题：
// TCP 是字节流，不保证 一次 read = 一条消息
// 实际 TCP 可能是这样的：
// 【粘包】两条消息粘在一起，一次 read 全读出来了：
//  第 1 次 read → {"msgid":1,"data":"ping"}{"msgid":1,"data":"ping"}
//  你的 json::parse 会失败！因为这不是合法的单条 JSON
//
// 【半包】一条消息被拆成两半：
//  第 1 次 read → {"msgid":1,"da
//  第 2 次 read → ta":"ping"}
//  两次都 parse 失败！
//
// 【混合】情况：
//  第 1 次 read → {"msgid":1,"data":"ping"}{"msgid":1,"d
//  一条完整的 + 半条，既粘包又半包
//
//
// 协议格式：
//   +----------+------------------+
//   | len (4B) |   JSON payload   |
//   +----------+------------------+
//
// len：payload 的字节数，uint32_t，网络字节序（大端）
// payload：JSON 文本（UTF-8）
//
//   htonl() = host to network long（主机序 → 网络序）
//   ntohl() = network to host long（网络序 → 主机序）

#include <muduo/net/Buffer.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Logging.h>

#include <string>
#include <functional>
#include <arpa/inet.h>   // htonl, ntohl

#include "json_helper.hpp"

using namespace muduo;
using namespace muduo::net;

// 将JSON字符串打包成协议格式，然后发送
inline void codecSend(const TcpConnectionPtr &conn, const std::string &message){
    Buffer buf;
    buf.append(message.data(), message.size());

    int32_t len = static_cast<int32_t>(message.size());
    buf.prepend(&len, sizeof(len));

    conn->send(&buf);
    
    LOG_DEBUG << "codecSend: len=" << message.size()
            << " payload=" << message;
}

// using 是 C++11 的类型别名，等价于老式的 typedef
// std::function 是一个可调用对象的包装器，尖括号里写的是函数签名。
// MessageCallback 是一个类型别名，代表
// "任何接收 (连接, 消息, 时间戳) 三个参数、无返回值的可调用对象"
using MyMessageCallback = std::function<void(
    const TcpConnectionPtr &,
    const std::string &,
    Timestamp)>;

//从Buffer中提取完整的协议格式消息
inline void codecOnMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time, const MyMessageCallback &msgCallback){
    //至少有4字节才能读出协议格式中的长度头
    while(buf->readableBytes() >= sizeof(int32_t)){

        // ① 偷看前 4 字节（不消费，不移动读指针）
        //    为什么用 peek 不用 read？
        //    因为可能 payload 还没到齐，这时候不能消费长度头
        //    等 payload 到齐了再一起消费
        const int32_t *p = reinterpret_cast<const int32_t *>(buf->peek());
        int32_t msgLen = *p;

        if(msgLen < 0 || msgLen > 65536){
            LOG_ERROR << "错误的消息长度：" << msgLen;
            conn->shutdown();
            return;
        }

        //检查buffer中的数据够不够一条完整消息
        if(buf->readableBytes() < sizeof(int32_t) + static_cast<int32_t>(msgLen)){
            // 不够 → 半包，跳出循环，等下次数据到达再来
            LOG_DEBUG << "Half packet: need " << msgLen
                      << " but only " << (buf->readableBytes() - sizeof(int32_t))
                      << " available";
            break;
        }

        //通过检查后就取出完整消息
        //先跳过4字节长度头
        buf->retrieve(sizeof(int32_t));
        std::string payload = buf->retrieveAsString(msgLen);

        LOG_DEBUG << "codecDecode: len=" << msgLen << " payload=" << payload;

        // ④ 调用业务回调（把解码后的 JSON 字符串交给上层处理）
        msgCallback(conn, payload, time);
    }
}


#endif