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

enum VhostUserRequest;

Client* new_client(const char* path);
int init_client(Client* client);
int end_client(Client* client);
int set_handler_client(Client* client, AppHandlers* handlers);
int loop_client(Client* client);

int vhost_ioctl(Client* client, enum VhostUserRequest request, ...);

#endif /* CLIENT_H_ */
