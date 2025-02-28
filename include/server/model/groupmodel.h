#ifndef GROUPMODEL_H
#define GROUPMODEL_H
#include "group.h"
#include <string>
#include <vector>
class GroupModel
{
public:
    //创建群组
    bool createGroup(Group& group);
    //加入群组
    void addGroup(int userid,int groupid,std::string role);
    //查询用户所在的群组信息
    std::vector<Group> queryGroups(int userid);
    //根据指定的groupid查询群组成员 除usedid自己 主要用户群聊业务给其他成员发送信息
    std::vector<int> queryGroupUsers(int userid,int groupid);
};

#endif