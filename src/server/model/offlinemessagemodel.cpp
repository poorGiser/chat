#include "offlinemessagemodel.h"
#include "db.h"

//增加用户的离线消息
void OffLineMessageModel::insert(int userid,std::string msg){
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage(userid,message) values(%d,'%s')", userid, msg.c_str());

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}
void OffLineMessageModel::remove(int userid){
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid=%d", userid);

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}
std::vector<std::string> OffLineMessageModel::query(int userid){
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid=%d",userid);
    std::vector<std::string> vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}
