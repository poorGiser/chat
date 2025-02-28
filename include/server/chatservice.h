#ifndef CHATSERVICE_H
#define CHATSERVICE_H
#include <unordered_map>
#include <mutex>
#include <functional>
#include <muduo/net/TcpConnection.h>

#include "usermodel.h"
#include "offlinemessagemodel.h"
#include "friendmodel.h"
#include "groupmodel.h"
#include "redis.h"

#include "json.hpp"


using namespace muduo;
using namespace muduo::net;
using namespace nlohmann;

//处理消息的事件回调方法类型
using MshHandler = std::function<void(const TcpConnectionPtr& conn,json& js,Timestamp time)>;

//聊天服务器业务类
class ChatService{
public:
    static ChatService* instance();

    //处理登录业务
    void login(const TcpConnectionPtr& conn,json& js,Timestamp time);

    //处理注册业务
    void reg(const TcpConnectionPtr& conn,json& js,Timestamp time);

    //处理聊天业务
    void oneChat(const TcpConnectionPtr& conn,json& js,Timestamp time);

    //添加好友业务
    void addFriend(const TcpConnectionPtr& conn,json& js,Timestamp time);

    //创建群组业务
    void createGroup(const TcpConnectionPtr& conn,json& js,Timestamp time);
    //加入群组业务
    void addGroup(const TcpConnectionPtr& conn,json& js,Timestamp time);
    //群组聊天业务
    void groupChat(const TcpConnectionPtr& conn,json& js,Timestamp time);

    //注销业务
    void loginout(const TcpConnectionPtr& conn,json& js,Timestamp time);

    //获取消息对应的处理器
    MshHandler getHandler(int msgId);

    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);

    //服务器异常，业务重置方法
    void reset();

    // 从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int, string);
private:
    ChatService();

    //存储消息id和其对应的业务处理办法
    std::unordered_map<int,MshHandler> _msgHandlerMap;
    
    //存储用户的通信连接
    std::unordered_map<int,TcpConnectionPtr> _userConnMap;

    //定义互斥锁，保证_userConnMap的线程安全
    std::mutex _connMutex;

    //数据操作类对象
    UserModel _userModel;
    //离线消息操作对象
    OffLineMessageModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    Redis _redis;
};

#endif