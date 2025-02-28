#include "chatservice.h"
#include "public.h"
#include <muduo/base/Logging.h>
#include <string>
#include <memory>
#include <map>
#include <iostream>
using namespace muduo;

ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});

    if (_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "login service!!!";

    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);

    if (user.getId() != -1 && user.getPassword() == pwd)
    {
        if (user.getState() == "online")
        {
            // 已经登录 不允许重复登录
            json response;
            response["msgId"] = LOGIN_MSG_ACK;
            response["error"] = 2;
            response["errorMsg"] = "该账号已经登陆,请重新输入新账号!";
            conn->send(response.dump());
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // 登录成功
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgId"] = LOGIN_MSG_ACK;
            response["id"] = user.getId();
            response["error"] = 0;
            response["name"] = user.getName();

            // 查询用户是否有离线消息
            std::vector<std::string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取离线消息后，删除之前的离线消息
                _offlineMsgModel.remove(id);
            }

            // 查询用户的好友信息并返回
            std::vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                std::vector<json> vec2;
                for (auto user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.emplace_back(js);
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组信息
            std::vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                std::vector<std::string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    std::vector<std::string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }

            //提交管道
            _redis.subscribe(id);

            conn->send(response.dump());
        }
    }
    else
    {
        json response;
        response["msgId"] = LOGIN_MSG_ACK;
        response["error"] = 1;
        response["errorMsg"] = "用户名或者密码错误";
        conn->send(response.dump());
    }
}

// 处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "reg service!!!";

    std::string name = js["name"];
    std::string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);

    bool state = _userModel.insert(user);

    if (state)
    {
        // 注册成功
        json response;
        response["msgId"] = REG_MSG_ACK;
        response["id"] = user.getId();
        response["error"] = 0;
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgId"] = REG_MSG_ACK;
        response["error"] = 1;
        conn->send(response.dump());
    }
}

// 添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "addFriend service!!!";

    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    _friendModel.insert(userid, friendid);
}

// 获取消息对应的处理器
MshHandler ChatService::getHandler(int msgId)
{
    auto it = _msgHandlerMap.find(msgId);

    if (it == _msgHandlerMap.end())
    {
        LOG_ERROR << "msgId:" << msgId << "can not find handler";
        return [](const TcpConnectionPtr &conn, json &js, Timestamp time) {};
        // 返回一个默认的处理器
    }
    else
    {
        return _msgHandlerMap[msgId];
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;

    {
        std::lock_guard<std::mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                // 从map表删除用户的链接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    if (user.getId() != -1)
    {
        // 更新用户的状态信息
        user.setState("offline");
        _userModel.updateState(user);
    }

    if (user.getId() != -1)
    {
        //注销管道
        _redis.unsubscribe(user.getId());
    }
}

// 服务器异常，业务重置方法
void ChatService::reset()
{    
    _userModel.resetState();
}

// 处理聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "one chat service!!!";

    int toid = js["toid"].get<int>();
    {
        std::lock_guard<std::mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // 在线
            it->second->send(js.dump());
            return;
        }
    }

    //在别的服务器
    // 查询toid是否在线 
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }

    // 不在线
    _offlineMsgModel.insert(toid, js.dump());
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    std::string name = js["name"];
    std::string desc = js["desc"];

    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}
// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}
// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    std::vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    std::lock_guard<std::mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end()){
            it->second->send(js.dump());
        }
        
        else{
            User user = _userModel.query(id);
            if (user.getState() == "online"){
                _redis.publish(id, js.dump());
            }
            else{
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        std::lock_guard<std::mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user;
    user.setId(userid);
    user.setState("offline");
    _userModel.updateState(user);
}

void ChatService::handleRedisSubscribeMessage(int userid, std::string msg){
    std::lock_guard<std::mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }
}