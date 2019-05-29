/*
 * server.h
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef SERVER_H_
#define SERVER_H_

#include <limits.h>

#include "common.h"
#include "unsock.h"
#include "fd_list.h"
#include "vhost_user.h"

struct ServerMsg {
    struct VhostUserMsg msg;
    size_t fd_num;
    int fds[VHOST_MEMORY_MAX_NREGIONS];
};

typedef struct ServerMsg ServerMsg;

int set_handler_server(UnSock* server, AppHandlers* handlers);
int init_server(UnSock* server, int is_listen);
int loop_server(UnSock* server);

#endif /* SERVER_H_ */
