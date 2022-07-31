#include"ChatServer.h"


int main()
{
    net::EventLoop loop;
    net::InetAddress addr("127.0.0.1",6666);
    ChatServer server(&loop,addr,"MyChatServer");


    server.start();     //  epoll_ctl 将 lfd 添加到epoll上      int epoll_ctl(int epfd , int op , int fd , struct epoll_event * event );  epfd = EPOLL_CTL_ADD     
    loop.loop();        //  epoll_wait以阻塞方式等待新用户的连接 和 已连接用户的读写事件等

    return 0;
}