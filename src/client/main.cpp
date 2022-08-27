#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <vector>
#include <iostream>
#include <string.h>
#include <assert.h>

#include <thread>
#include "json.hpp"
#include "User.hpp"
#include "Group.hpp"
#include "public.h"

using nlohmann::json;
using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::vector;

//  记录当前系统登陆的用户信息
User current_user;
//  记录当前用户的好友信息
vector<User> current_user_friendlist;
//  记录当前登录用户的群组列表信息
vector<Group> current_user_grouplist;

//  显示当前登录成功的用户信息
void showCurrentUserData();
//  接收消息线程
void recvTaskHandler(int);
//  获取系统时间
string getCurrentTime();
//  主聊天页面程序
void showMainMenu();

//  聊天客户端实现，main线程用作发送消息线程，子线程用作接收线程
int main(int argc, char *argv[])
{
    cout << "argc " << argc << endl;
    if (argc != 3)
    {
        cout << "Usage : "
             << "ChatClient 127.0.0.1 "
             << " 6666 " << endl;
        exit(1);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    //  1.  创建socket
    int client_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(client_fd >= 0);

    //  2.  创建要连接的server 结构体address
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;              //  ip类型
    inet_pton(AF_INET, ip, &server_address.sin_addr); //  IP
    server_address.sin_port = htons(port);            //  port

    //  3.  发起连接
    int ret = connect(client_fd, (sockaddr *)&server_address, sizeof server_address);
    if (ret == -1)
    {
        cout << "client failed to connect server" << endl;
        exit(1);
    }

    while (1)
    {
        cout << "======================MENU======================" << endl;
        cout << "1. login " << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "input your choice : ";
        //  读取选择
        int ch;
        cin >> ch;
        cin.get(); //  读掉缓冲区残留的回车

        switch (ch)
        {
        //  login登录业务
        case 1:
        {
            int id = 0;
            char pwd[50] = {0};

            cout << "id : ";
            cin >> id;
            cin.get();
            cout << "password : ";
            cin.getline(pwd, 50);

            json send_js;
            send_js["msg_id"] = LGOIN_MSG;
            send_js["id"] = id;
            send_js["password"] = pwd;
            string request = send_js.dump();

            int ret = send(client_fd, request.c_str(), request.size(), 0);
            if (ret == -1)
            {
                cout << "client failed to send login msg" << endl;
                break;
            }
            else
            {
                char recv_buf[1024] = {0};
                int len = recv(client_fd, recv_buf, 1024, 0);
                if (len == -1)
                {
                    cout << "client failed to recv response for login" << endl;
                    break;
                }
                else
                {
                    //  序列化成json
                    json res_js = json::parse(recv_buf);
                    //  登陆失败
                    if (res_js["errno"].get<int>() == 2)
                    {
                        cerr << "client failed to login" << endl;
                        cerr << res_js["errmsg"].get<string>() <<endl;
                        break;
                    }
                    //  登陆成功
                    else
                    {
                        current_user.setId(res_js["id"].get<int>());
                        current_user.setName(res_js["name"]);
                        current_user.setPwd(pwd);
                        current_user.setState("online");

                       
                        //  a. 存下好友列表
                        vector<string> friend_list = res_js["friends"];
                        cout<<"data debug friend_list "<<friend_list.size()<<endl;
                        if (!friend_list.empty())
                        {
                            for (string &s : friend_list)
                            {
                                //  利用json格式传送的数据，来构造user对象
                                json js = json::parse(s);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"].get<string>());
                                user.setState(js["state"]);

                                //  存好友
                                current_user_friendlist.push_back(user);
                            }
                        }

                        //  b. 存下群组列表
                        vector<string> group_list = res_js["groups"];
                        cout<<"data debug group_list "<<group_list.size()<<endl;
                        if (!group_list.empty())
                        {
                            for (string &s : group_list)
                            {
                                Group g;
                                json js = json::parse(s);
                                g.setId(js["group_id"].get<int>());
                                g.setName(js["group_name"]);
                                g.setDesc(js["group_desc"]);
                                vector<string> group_users = js["group_users"];
                                for (string &s : group_users)
                                {
                                    json js = json::parse(s);
                                    GroupUser u;
                                    u.setId(js["id"]);
                                    u.setName(js["name"]);
                                    u.setRole(js["role"]);
                                    g.getUsers().push_back(u);
                                }
                                //  存组
                                current_user_grouplist.push_back(g);
                            }
                        }

                        //  c. offline msg
                        vector<string> vec_offlinemsg = res_js["offlinemsg"];
                        if (!vec_offlinemsg.empty())
                        {
                            for (string &s : vec_offlinemsg)
                            {
                                //  序列化
                                json js = json::parse(s);
                                cout << js["time"]
                                     << " [" << js["id"] << "] " << js["name"]
                                     << " said: " << js["msg"] << endl;
                            }
                        }

                        //  d. 开启接收线程
                        std::thread recvTaskThread(recvTaskHandler, client_fd);
                        recvTaskThread.detach();
                        
                        //  e. 显示登录用户的基本信息
                        showCurrentUserData();
                       
                        //  f. 聊天界面
                        showMainMenu();
                    }
                }
            }
            break;
        }
        //  register 注册业务
        case 2:
        {
            //  id是serve分配的
            //  name,password是client设置的
            string name;
            string password;
            cout << "input your name : ";
            cin >> name;
            cout << "input your password : ";
            cin >> password;
            
            json send_js;
            send_js["msg_id"] = REG_MSG;
            send_js["name"] = name;
            send_js["password"] = password;
            string request = send_js.dump();

            int ret = send(client_fd, request.c_str(), request.size(), 0);
            if (ret == -1)
            {
                cout << "client failed to send register msg" << endl;
                break;
            }
            else
            {
                char buf[1024] = {0};
                int len = recv(client_fd, buf, sizeof buf, 0);
                if (len == -1)
                {
                    cout << "client failed to recv response for register" << endl;
                    break;
                }
                else
                {
                    json res_js = json::parse(buf);
                    if (res_js["errno"].get<int>() == 1)
                    {
                        cerr << "client failed to register" << endl;
                        cerr << res_js["errmsg"] << endl;
                        break;
                    }
                    else
                    {
                        cout << "Register Success! Please remember your id : " << res_js["id"].get<int>() << endl;
                        break;
                    }
                }
            }
            break;
        }
        //  quit
        case 3:
        {
            close(client_fd);
            current_user_friendlist.clear();
            current_user_grouplist.clear();
            exit(0);
            break;
        }
        default:
            cout << "invalid input!" << endl;
        }
    }
}

void showCurrentUserData()
{
    cout << "======================Current User Data======================" << endl;

    cout << "current login user ---> id : " << current_user.getId() << " name : " << current_user.getName() << endl;

    cout << "======================Friend List======================" << endl;
    for (const User &u : current_user_friendlist)
    {
        cout << u.getId() << " " << u.getName() << " " << u.getState() << endl;
    }

    cout << "======================Group List======================" << endl;
    for (Group &g : current_user_grouplist)
    {
        cout << g.getId() << " " << g.getName() << " " << g.getDesc() << endl;
        for (GroupUser &u : g.getUsers())
        {
            cout << u.getId() << " " << u.getName() << " " << u.getRole() << endl;
        }
    }
}

void showMainMenu()
{
}

string getCurrentTime()
{
}

void recvTaskHandler(int client_fd)
{
}

//  1. 好习惯 cin.get()
//  从缓冲区读了一个整数
//  接着要读掉缓冲区中的回车 cin.get();
//  因为我们键入时键入的是 int 和 一个 回车
//  如果我们不读取这个残留的回车，那么流中下一个如果想读的是字符串就读不到了，只能读到一个回车。

//   cin >> name不能读取空格 遇到空格就会结束读取
//  所以读用户名应当用 cin.getline() 遇到回车结束读取

//  好友列表、组列表其实应当存在客户端