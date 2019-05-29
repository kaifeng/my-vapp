/*
 * client.h
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef CLIENT_H_
#define CLIENT_H_

#include <limits.h>
#include "common.h"
#include "fd_list.h"
#include "unsock.h"

enum VhostUserRequest;

int init_client(UnSock* client);
int set_handler_client(UnSock* client, AppHandlers* handlers);
int loop_client(UnSock* client);

int vhost_ioctl(UnSock* client, enum VhostUserRequest request, ...);

#endif /* CLIENT_H_ */
