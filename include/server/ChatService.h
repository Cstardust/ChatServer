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
    //  登陆业务
    void login(const net::TcpConnectionPtr& , json &,Timestamp);
    //  注册业务
    void reg(const net::TcpConnectionPtr& , json &,Timestamp);
    //  根据消息类型返回回调函数
    MsgHandler getMsgHandler(const MsgType &msg_type) const;
    //  端到端通信业务
    void toPChat(const net::TcpConnectionPtr& , json &,Timestamp);
    //  处理客户端断开
    void clientClose(const net::TcpConnectionPtr&);

private:
    ChatService();
private:
    //  map存储消息id和相应的handler
    unordered_map<MsgType,MsgHandler> msgHandlerTable_;
    //  数据模块提供给业务模块的一个handler
    //  数据库User表的操作对象
    UserModel userModel_;
    //  记录user连接信息
    unordered_map<int,net::TcpConnectionPtr> userConnTable_;
    //  多线程共享userConnTable_的互斥锁
    std::mutex connMtx_;
};


#endif


