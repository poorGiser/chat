#include "redis.h"
#include <iostream>
Redis::Redis():_publish_context(nullptr),_subcribe_context(nullptr){

}
Redis::~Redis(){
    if(_publish_context != nullptr){
        redisFree(_publish_context);
    }
    if(_subcribe_context != nullptr){
        redisFree(_subcribe_context);
    }
}

//连接redis服务器
bool Redis::connect(){
    _publish_context = redisConnect("127.0.0.1",6379);
    if(_publish_context == nullptr){
        std::cerr << "connect redis failed!" << std::endl;
        return false;
    }

    _subcribe_context = redisConnect("127.0.0.1",6379);
    if(_subcribe_context == nullptr){
        std::cerr << "connect redis failed!" << std::endl;
        return false;
    }

    //在单独的线程中，监听通道上的事件，有消息给业务层上报
    std::thread t([&](){
        observer_channel_message();
    });

    t.detach();

    std::cout << "connect redis-server success" << std::endl;
    return true;
}

bool Redis::publish(int channel,std::string message){
    redisReply* reply = (redisReply*) redisCommand(_publish_context,"PUBLISH %d %s",channel,message.c_str());
    if(nullptr == reply){
        std::cerr << "publish command failed!" << std::endl;
        return false;
    }

    freeReplyObject(reply);
    return true;
}

bool Redis::subscribe(int channel){
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "SUBSCRIBE %d", channel))
    {
        std::cerr << "subscribe command failed!" << std::endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            std::cerr << "subscribe command failed!" << std::endl;
            return false;
        }
    }
    // redisGetReply

    return true;
}

bool Redis::unsubscribe(int channel){
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "UNSUBSCRIBE %d", channel))
    {
        std::cerr << "unsubscribe command failed!" << std::endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            std::cerr << "unsubscribe command failed!" << std::endl;
            return false;
        }
    }
    return true;
}

void Redis::observer_channel_message(){
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(this->_subcribe_context, (void **)&reply))
    {
        // 订阅收到的消息是一个带三元素的数组
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道上发生的消息
            _notify_message_handler(atoi(reply->element[1]->str) , reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    std::cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<" << std::endl;
}

void Redis::init_notify_handler(std::function<void(int,std::string)> fn){
    this->_notify_message_handler = fn;
}