#ifndef _CHAT_SERVICE_H_
#define _CHAT_SERVICE_H_

#include<unordered_map>
#include<functional>
#include<muduo/net/TcpConnection.h>
#include<muduo/net/Buffer.h>
#include<muduo/base/Logging.h>
#include<mutex>
#include"json.hpp"
#include"public.h"
#include"UserModel.h"
#include"OfflineMessageModel.h"
#include"FriendModel.h"
#include"GroupModel.h"

using namespace muduo;
using json = nlohmann::json;
using std::unordered_map;
//  处理消息的事件回调方法类型
using MsgHandler = std::function<void(const net::TcpConnectionPtr & , json & , Timestamp )>;


//  ChatServer的业务模块
//  使用单例模式
class ChatService{
public:
    //  单例模式
    static ChatService * getInstance();
    //  根据消息类型返回回调函数
    MsgHandler getMsgHandler(const MsgType &msg_type) const;
    //  登陆业务
    void login(const net::TcpConnectionPtr& , json &,Timestamp);
    //  注册业务
    void reg(const net::TcpConnectionPtr& , json &,Timestamp);
    //  端到端通信业务
    void toPChat(const net::TcpConnectionPtr& , json &,Timestamp);
    //  加好友业务
    void addFriend(const net::TcpConnectionPtr& , json &,Timestamp);
    //  创建群组业务
    void createGroup(const net::TcpConnectionPtr& , json &,Timestamp);
    //  加群组业务    
    void addIntoGroup(const net::TcpConnectionPtr& , json &,Timestamp);
    //  群组聊天业务
    void groupChat(const net::TcpConnectionPtr& , json &,Timestamp);    
    //  处理客户端断开
    void clientClose(const net::TcpConnectionPtr&);
    //  处理Server异常终止
    void reset();


private:
    ChatService();
private:
    //  map存储消息id和相应的handler
    unordered_map<MsgType,MsgHandler> msgHandlerTable_;
    //  记录user连接信息
    unordered_map<int,net::TcpConnectionPtr> userConnTable_;
    //  多线程共享userConnTable_的互斥锁
    std::mutex connMtx_;

    //  数据操作工具类对象
    //  数据模块提供给业务模块的一个handler
        //  数据库User表的操作对象
        UserModel userModel_;
        //  离线消息表
        OfflineMsgModel OfflineMsgModel_;
        //  好友关系表
        FriendModel FriendModel_;
        //  群组关系表s
        GroupModel groupModel_;
};

#endif


