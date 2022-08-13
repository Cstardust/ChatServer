#include "ChatService.h"

//  单例模式
ChatService *ChatService::getInstance()
{
    static ChatService chatserivce_instance;
    return &chatserivce_instance;
}

//  注册msg_type 以及 相应handler
ChatService::ChatService()
{
    msgHandlerTable_.insert({MsgType::LGOIN_MSG, std::bind(&ChatService::login, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    msgHandlerTable_.insert({MsgType::REG_MSG, std::bind(&ChatService::reg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    msgHandlerTable_.insert({MsgType::PTOP_CHAT_MSG,std::bind(&ChatService::toPChat,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3)});
    msgHandlerTable_.insert({MsgType::ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3)});
}

MsgHandler ChatService::getMsgHandler(const MsgType &msg_type) const
{
    unordered_map<MsgType, MsgHandler>::const_iterator iter = msgHandlerTable_.find(msg_type);

    if (iter == msgHandlerTable_.end())
    { //  不存在相应msg_id的handler
        return [=](const net::TcpConnectionPtr &, json &, Timestamp)
        {
            LOG_ERROR << "msg_id : " << msg_type << "can not find handler";
        };
    }
    else
    {
        return iter->second;
    }
}

void ChatService::login(const net::TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "do login service ! ";

    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = userModel_.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        //  用户存在 && 密码正确
        if (user.getState() == "online")
        {
            //  该用户已登录 不可重复登录
            json res;
            res["msg_id"] = LGOIN_MSG_ACK;
            res["errno"] = 2;
            res["id"] = user.getId();
            res["errmsg"] = "该账号已经登陆 请输入其他账号";

            conn->send(res.dump());
        }
        else
        {
            //  登陆成功 记录user连接信息
            //  线程安全
            //  操作多个线程共享对象 上锁
            {
                std::lock_guard<std::mutex> lock(connMtx_);
                userConnTable_.insert({id, conn});
            }

            //  登录成功 更新用户状态
            user.setState("online");
            userModel_.upState(user);

            //  登录成功信息
            json res;
            res["msg_id"] = LGOIN_MSG_ACK;
            res["errno"] = 0;
            res["id"] = id;
            res["name"] = user.getName();
                //  发送离线信息
            res["msg"] = OfflineMsgModel_.query(id);
            
                //  登录时，加载好友列表
            vector<User> friends = FriendModel_.query(id);
            vector<string> friends_list;
            for(User &usr : friends)
            {
                json js;
                js["id"] = usr.getId();
                js["name"] = usr.getName();
                js["state"] = usr.getState();
                friends_list.push_back(js.dump());
            }            
            res["friends"] = friends_list;

            //  发送消息
            conn->send(res.dump());
            
                //  发送完了 删除离线信息
            OfflineMsgModel_.remove(id);
            
        }
    }
    else
    {
        //  密码不正确 || 用户不存在 || ChatServer连接 SqlServer失败
        json res;
        res["msg_id"] = LGOIN_MSG_ACK;
        res["errno"] = 2;
        res["id"] = user.getId();
        conn->send(res.dump());
    }
}

//  处理注册业务
void ChatService::reg(const net::TcpConnectionPtr &conn, json &js, Timestamp time)
{
    LOG_INFO << "do reg service ! ";

    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool st = userModel_.insert(user);
    if (st)
    { //  注册成功
        json repo;
        repo["msg_id"] = REG_MSG_ACK; //  消息类型
        repo["errno"] = 0;            //  0 注册成功
        repo["id"] = user.getId();
        conn->send(repo.dump()); //  发送消息
    }
    else
    {
        //  注册失败
        json repo;
        repo["msg_id"] = REG_MSG_ACK;
        repo["errno"] = 1; //  1 注册失败
        conn->send(repo.dump());
    }
}

void ChatService::clientClose(const net::TcpConnectionPtr &conn)
{
    //  断开客户处理
    //  1. 从table中删除TcpConnectionPtr 和 id
    User user;
    {
        //  要操作table 所以上锁
        std::lock_guard<std::mutex> guard(connMtx_);
        for (unordered_map<int, net::TcpConnectionPtr>::iterator iter = userConnTable_.begin();
             iter != userConnTable_.end();
             ++iter)
        {
            if (iter->second == conn)
            {
                //  移除该用户连接记录
                userConnTable_.erase(iter);
                //  记录该用户的主键
                user.setId(iter->first);
                break;
            }
        }
    }

    //  2. 用户状态信息 online -> offline
    if(user.getId()!=-1)    //  用户在线
    {
        user.setState("offline");
        userModel_.upState(user);
    }
    return;
}

void ChatService::toPChat(const net::TcpConnectionPtr &conn, json &js, Timestamp time)
{
    js["msg_id"] = PTOP_CHAT_MSG_ACK;    //  server添加的ack
    int to_id = js["to_id"].get<int>();

    //  根据id寻找conn来判断是否在线
        //  对方在线
            //  直接转发 服务器主动推送消息给to_id用户
        //  不在线
            //  离线存储
    {
        std::lock_guard<std::mutex> guard(connMtx_); //  因为要操作userConnTable_ 所以上锁
        const unordered_map<int, net::TcpConnectionPtr>::iterator iter = userConnTable_.find(to_id);
        if (iter != userConnTable_.end())
        {
            //  对方在线 直接转发
            //  转发动作放在锁里面的原因
                //  若放在外面 则可能pToPChat释放锁之后,其他线程又执行了clientClose，发送消息的用户可能就下线了 无法转发。
            iter->second->send(js.dump());
            return;
        }
    }

    //  不在线 离线
    OfflineMsgModel_.insert(to_id,js.dump());
}


void ChatService::reset()
{
    userModel_.resetState();
}

void ChatService::addFriend(const net::TcpConnectionPtr& conn, json & js,Timestamp time)
{
    int user_id = js["id"].get<int>();
    int friend_id = js["friend_id"].get<int>();
    //  向表中插入这对好友
    FriendModel_.insert(user_id,friend_id);
    FriendModel_.insert(friend_id,user_id);    
}

//  创建群组业务
void ChatService::createGroup(const net::TcpConnectionPtr& conn, json & js,Timestamp time)
{
    //  相关信息
    int user_id = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //  存储创建的群组信息（存入数据库）
    Group group(-1,name,desc);
    if(groupModel_.createGroup(group))
    {
        //  存储群组创建人信息
        groupModel_.addIntoGroup(user_id,group.getId(),"creator");
    }
}


//  加入群组业务 -》addIntoModel
void ChatService::addIntoGroup(const net::TcpConnectionPtr& conn, json & js,Timestamp time)
{
    int user_id = js["id"].get<int>();
    int group_id = js["groupid"].get<int>();
    groupModel_.addIntoGroup(user_id,group_id,"normal");
}


//  群组聊天业务
void ChatService::groupChat(const net::TcpConnectionPtr& conn, json & js,Timestamp time)
{
    int user_id = js["id"].get<int>();
    int group_id = js["groupid"].get<int>();
    //  根据user_id group_id 得到应转发的所有用户
    vector<int> users = groupModel_.queryGroupUsers(user_id,group_id);
    
    //  将消息转发给所有用户
    std::lock_guard<std::mutex> guard(connMtx_);        //  要操作map 上锁保护
    for(auto id : users)
    {
        unordered_map<int,net::TcpConnectionPtr> ::const_iterator iter = userConnTable_.find(id);
        if(iter==userConnTable_.end())
        {
            //  转发群消息
            iter->second->send(js.dump());
        } 
        else
        {
            //  存储离线消息
            OfflineMsgModel_.insert(id,js.dump());
        }
    }
}

