#include "redis.hpp"
#include <muduo/base/Logging.h>
#include <thread>

Redis::Redis() : publishContext_(nullptr), subscribeContext_(nullptr) {}

Redis::~Redis() {
    if (publishContext_) {
        redisFree(publishContext_);
    }
    if (subscribeContext_) {
        redisFree(subscribeContext_);
    }
}

bool Redis::connect() {
    publishContext_ = redisConnect("127.0.0.1", 6379);
    if (publishContext_ == nullptr || publishContext_->err) {
        if (publishContext_) {
            LOG_ERROR << "Redis publish connect error: " << publishContext_->errstr;
            redisFree(publishContext_);
            publishContext_ = nullptr;
        }
        return false;
    }

    subscribeContext_ = redisConnect("127.0.0.1", 6379);
    if (subscribeContext_ == nullptr || subscribeContext_->err) {
        if (subscribeContext_) {
            LOG_ERROR << "Redis subscribe connect error: " << subscribeContext_->errstr;
            redisFree(subscribeContext_);
            subscribeContext_ = nullptr;
        }
        return false;
    }

    std::thread([this]() {
        init_notify_handler();
    }).detach();

    LOG_INFO << "Redis connect success";
    return true;
}

bool Redis::publish(int channel, const std::string& message) {
    redisReply* reply = (redisReply*)redisCommand(publishContext_, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr) {
        LOG_ERROR << "Redis publish command failed";
        return false;
    }
    freeReplyObject(reply);
    return true;
}

bool Redis::subscribe(int channel) {
    if (redisAppendCommand(subscribeContext_, "SUBSCRIBE %d", channel) != REDIS_OK) {
        LOG_ERROR << "Redis subscribe command failed";
        return false;
    }
    int done = 0;
    while (!done) {
        if (redisBufferWrite(subscribeContext_, &done) != REDIS_OK) {
            LOG_ERROR << "Redis subscribe buffer write failed";
            return false;
        }
    }
    return true;
}

bool Redis::unsubscribe(int channel) {
    if (redisAppendCommand(subscribeContext_, "UNSUBSCRIBE %d", channel) != REDIS_OK) {
        LOG_ERROR << "Redis unsubscribe command failed";
        return false;
    }
    int done = 0;
    while (!done) {
        if (redisBufferWrite(subscribeContext_, &done) != REDIS_OK) {
            LOG_ERROR << "Redis unsubscribe buffer write failed";
            return false;
        }
    }
    return true;
}

void Redis::observer_channel_message(std::function<void(int, const std::string&)> fn) {
    notifyMessageHandler_ = fn;
}

void Redis::init_notify_handler() {
    while (true) {
        redisReply* reply = (redisReply*)redisCommand(subscribeContext_, "");
        if (reply == nullptr) {
            LOG_ERROR << "Redis subscribe context error";
            break;
        }

        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3) {
            if (std::string(reply->element[0]->str) == "message") {
                if (notifyMessageHandler_) {
                    notifyMessageHandler_(std::atoi(reply->element[1]->str), reply->element[2]->str);
                }
            }
        }
        freeReplyObject(reply);
    }
}
