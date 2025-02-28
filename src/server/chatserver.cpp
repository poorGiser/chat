#include "charserver.h"
#include <functional>
#include <string>
#include "json.hpp"
#include "chatservice.h"

using json = nlohmann::json;

// 初始化服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg) : _server(loop, listenAddr, nameArg), _loop(loop)
{
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, std::placeholders::_1));
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void ChatServer::start()
{
    _server.start();
}

// 上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    //用户断开连接
    if(!conn->connected()){
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }

}

// 上报读写事件信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    //数据的反序列化
    json js = json::parse(buf);

    //达到的目的：完全解耦网络模块的代码和业务模块的代码
    //通过js["msgid"]获取=》业务handler-=》conn js time
    auto handler = ChatService::instance()->getHandler(js["msgId"].get<int>());
    //执行相应的业务处理
    handler(conn,js,time);
}