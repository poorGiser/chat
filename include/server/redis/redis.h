#ifndef REDIS_H
#define REDIS_H
#include <thread>
#include <hiredis/hiredis.h>
#include <functional>

class Redis{
    public:
        Redis();
        ~Redis();

        //连接redis服务器
        bool connect();

        bool publish(int channel,std::string message);

        bool subscribe(int channel);

        bool unsubscribe(int channel);

        void observer_channel_message();

        void init_notify_handler(std::function<void(int,std::string)> fn);
    private:
        //hiredis同步上下文对象，负责publish消息
        redisContext* _publish_context;

        //hiredis同步上下文对象，负责subscribe消息
        redisContext* _subcribe_context;

        //回调操作，收到订阅的消息，给service上报
        std::function<void(int,std::string)> _notify_message_handler;
};
#endif