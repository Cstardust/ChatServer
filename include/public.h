#ifndef _PUBLIC_H_
#define _PUBLIC_H_

// client 和 server的公共头文件

enum MsgType{
    LGOIN_MSG = 1,
    LGOIN_MSG_ACK,
    REG_MSG,
    REG_MSG_ACK,
    PTOP_MSG,
};

#endif