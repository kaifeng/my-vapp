#ifndef UNSOCK_H_
#define UNSOCK_H_

#include <limits.h>

#include "common.h"
#include "unsock.h"
#include "fd_list.h"
#include "vhost_user.h"

struct ServerMsg;

typedef int (*InMsgHandler)(void* context, struct ServerMsg* msg);
typedef int (*PollHandler)(void* context);

// 处理socket消息的回调
typedef struct {
    void* context;      // vhost_server或vhost_client，传给handler使用
    InMsgHandler in_handler;
    PollHandler poll_handler;
} AppHandlers;

/* struct for maintaining a socket endpoint */
typedef struct {
    char sock_path[PATH_MAX + 1];    // unix domain socket path
    int sock;
    int is_connected;  // socket是否已连接
    int is_server;  // 是否为server端，server端负责清理socket path
    FdList fd_list;
    // 处理socket消息的回调
    void *context;  // vhost_server或vhost_client，传给handler使用
    InMsgHandler in_handler;
    PollHandler poll_handler;
} UnSock;

struct ServerMsg {
    struct VhostUserMsg msg;
    size_t fd_num;
    int fds[VHOST_MEMORY_MAX_NREGIONS];
};

typedef struct ServerMsg ServerMsg;

UnSock* new_unsock(const char* path);
int close_unsock(UnSock* s);
int init_server(UnSock* unsock, int is_listen);
int init_client(UnSock* unsock);

#endif
