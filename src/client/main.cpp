#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "group.h"
#include "user.h"
#include "public.h"

using namespace std;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 接收线程
void readTaskHandler(int clientfd);

// 获取系统时间
string getCurrentTime();

// 记录登录状态
// atomic_bool g_isLoginSuccess{false};

// 聊天主页面
void mainMenu(int clientfd);

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "Command invalid! example:./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }
    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }else{
        cout << "socket create success" << endl;
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login业务
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgId"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            // g_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);

            // cout << len << endl;

            if (len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0); // 阻塞了
                if (-1 == len)
                {
                    cerr << "recv login response error:" << request << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    // cout << responsejs << endl;
                    if (0 != responsejs["error"].get<int>()) // 登录失败
                    {
                        cerr << responsejs["errorMsg"] << endl;
                        // g_isLoginSuccess = false;
                    }
                    else // 登录成功
                    {
                        // 记录当前用户的id和name
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);

                        // 记录当前用户的好友列表信息
                        if (responsejs.contains("friends"))
                        {
                            // 初始化
                            g_currentUserFriendList.clear();

                            vector<json> vec = responsejs["friends"];
                            for (json &js : vec)
                            {
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }

                        // 记录当前用户的群组列表信息
                        if (responsejs.contains("groups"))
                        {
                            // 初始化
                            g_currentUserGroupList.clear();

                            vector<string> vec1 = responsejs["groups"];
                            for (string &groupstr : vec1)
                            {
                                json grpjs = json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                vector<string> vec2 = grpjs["users"];
                                for (string &userstr : vec2)
                                {
                                    GroupUser user;
                                    json js = json::parse(userstr);
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }

                                g_currentUserGroupList.push_back(group);
                            }
                        }

                        // cout << "get groups success!" << endl;

                        // 显示登录用户的基本信息
                        showCurrentUserData();

                        // 显示当前用户的离线消息  个人聊天信息或者群组消息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> vec = responsejs["offlinemsg"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                // time + [id] + name + " said: " + xxx
                                if (ONE_CHAT_MSG == js["msgId"].get<int>())
                                {
                                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                         << " said: " << js["msg"].get<string>() << endl;
                                }
                                else
                                {
                                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                         << " said: " << js["msg"].get<string>() << endl;
                                }
                            }
                        }

                        // 连接服务器成功，启动接收子线程
                        static int inNumber = 0;
                        inNumber++;
                        if(inNumber == 1){
                            std::thread readTask(readTaskHandler, clientfd); // pthread_create
                            readTask.detach();                               // pthread_detach
                        }

                        isMainMenuRunning = true;
                        mainMenu(clientfd);
                    }
                }
            }
        }
        break;
        case 2: // register业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgId"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            cout << request << endl;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }else{
                cout << clientfd << endl;
                cout << "发送成功" << endl;
            }

            // sem_wait(&rwsem); // 等待信号量，子线程处理完注册消息会通知
        }
        break;
        case 3: // quit业务
            close(clientfd);
            // sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

void showCurrentUserData()
{
    cout << "======================login user======================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}


// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}};

void help(int fd = 0, string str = "");
void chat(int fd = 0, string str = "");
void addfriend(int fd = 0, string str = "");
void creategroup(int fd = 0, string str = "");
void addgroup(int fd = 0, string str = "");
void groupchat(int fd = 0, string str = "");
void loginout(int fd = 0, string str = "");

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout},
};

void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法
    }
}
void readTaskHandler(int clientfd)
{
    for(;;){
        char buffer[1024] = {0};
        int len = recv(clientfd,buffer,1024,0);

        if(len == -1 || len == 0){
            close(clientfd);
            exit(-1);
        } 

        //接收ChatServer接收的消息
        json js = json::parse(buffer);
        if(ONE_CHAT_MSG == js["msgId"].get<int>()){
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
            << " said: " << js["msg"].get<string>() << endl;
            continue;
        }else if(GROUP_CHAT_MSG == js["msgId"].get<int>()){
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
            << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

// "help" command handler
void help(int, string)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgId"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}

void chat(int clientfd, string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgId"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

void creategroup(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "creategroup command invalid" << endl;
        return;
    }

    string groupname = str.substr(0,idx);
    string groupdesc = str.substr(idx + 1,str.size() - idx - 1);
    
    json js;
    js["id"] = g_currentUser.getId();
    js["msgId"] = CREATE_GROUP_MSG;
    js["name"] = groupname;
    js["desc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "creategroup error -> " << buffer << endl;
    }
}

void addgroup(int clientfd, string str){
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}

void groupchat(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr << "groupchat command invalid" << endl;
        return;
    }

    int groupid = atoi(str.substr(0,idx).c_str());
    string message = str.substr(idx + 1,str.size() - idx - 1);
    
    json js;
    js["id"] = g_currentUser.getId();
    js["msgId"] = GROUP_CHAT_MSG;
    js["groupid"] = groupid;
    js["msg"] = message;
    js["name"] = g_currentUser.getName();
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "creategroup error -> " << buffer << endl;
    }
}

void loginout(int clientfd, string str){
    json js;
    js["id"] = g_currentUser.getId();
    js["msgId"] = LOGINOUT_MSG;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "loginout error -> " << buffer << endl;
    }else{
        isMainMenuRunning =false;
    }
}