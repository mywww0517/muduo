#pragma once
#include <hiredis/hiredis.h>
#include <functional>
#include <string>

class Redis {
public:
    Redis();
    ~Redis();

    bool connect();
    bool publish(int channel, const std::string& message);
    bool subscribe(int channel);
    bool unsubscribe(int channel);
    void observer_channel_message(std::function<void(int, const std::string&)> fn);
    void init_notify_handler();

private:
    redisContext* publishContext_;
    redisContext* subscribeContext_;
    std::function<void(int, const std::string&)> notifyMessageHandler_;
};
