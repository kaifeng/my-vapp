/*
 * server.c
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>

#include "server.h"

typedef enum {
    ServerSockInit,
    ServerSockAccept,
    ServerSockDone,
    ServerSockError

} ServerSockStatus;

static int receive_sock_server(FdNode* node);
static int accept_sock_server(FdNode* node);

/* alloc a new socket server struct, initialize state to INSTANCE_CREATED */
Server* new_server(const char* path)
{
    Server* server = (Server*) calloc(1, sizeof(Server));

    //TODO: handle errors here

    strncpy(server->path, path ? path : VHOST_SOCK_NAME, PATH_MAX);
    server->status = INSTANCE_CREATED;

    return server;
}

/* initialize socket, start service or connect.
 * on sucess, server state -> INSTANCE_INITIALIZED
 */
int init_server(Server* server, int is_listen)
{
    struct sockaddr_un un;
    size_t len;

    if (server->status != INSTANCE_CREATED)
        return 0;

    // Create the socket
    if ((server->sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, server->path);

    len = sizeof(un.sun_family) + strlen(server->path);

    if (is_listen) {
        unlink(server->path); // remove if exists

        // Bind
        if (bind(server->sock, (struct sockaddr *) &un, len) == -1) {
            perror("bind");
            return -1;
        }

        // Listen
        if (listen(server->sock, 1) == -1) {
            perror("listen");
            return -1;
        }
    } else {

        if (connect(server->sock, (struct sockaddr *) &un, len) == -1) {
            perror("connect");
            return -1;
        }
    }

    /* init a fd_list struct, reset all fds and handler */
    init_fd_list(&server->fd_list, FD_LIST_SELECT_5);
    /* pick an unused fd node (fd = -1) and add sock as well as handler to it.
     * if the server is listening, read means a connection is coming in,
     * otherwise, it means a buffer is comming in.
     */
    add_fd_list(&server->fd_list, FD_READ, server->sock, (void*) server,
            is_listen?accept_sock_server:receive_sock_server);

    server->status = INSTANCE_INITIALIZED;

    return 0;
}

/* close socket, server state -> INSTANCE_END */
int end_server(Server* server)
{
    if (server->status != INSTANCE_INITIALIZED)
        return 0;

    // Close and unlink the socket
    // TODO: there is no unlink here)
    close(server->sock);

    server->status = INSTANCE_END;

    return 0;
}

/* there is data coming in from socket
 * further logic continues at common.c vhost_user_recv_fds and vhost_user_send_fds
 */
static int receive_sock_server(FdNode* node)
{
    Server* server = (Server*) node->context;
    int sock = node->fd;
    ServerMsg msg;
    ServerSockStatus status = ServerSockAccept;
    int r;

    msg.fd_num = sizeof(msg.fds)/sizeof(int);

    // Receive data from the other side
    r = vhost_user_recv_fds(sock, &msg.msg, msg.fds, &msg.fd_num);
    if (r < 0) {
        perror("recv");
        status = ServerSockError;
    } else if (r == 0) {
        status = ServerSockDone;
        del_fd_list(&server->fd_list, FD_READ, sock);
        close(sock);
    }

    if (status == ServerSockAccept) {
#ifdef DUMP_PACKETS
        dump_vhostmsg(&msg.msg);
#endif
        r = 0;
        // Handle the packet to the registered server backend
        // see vhost_server in_msg_server()
        if (server->handlers.in_handler) {
            void* ctx = server->handlers.context;
            r = server->handlers.in_handler(ctx, &msg);
            if (r < 0) {
                fprintf(stderr, "Error processing message: %s\n",
                        cmd_from_vhost_request(msg.msg.request));
                status = ServerSockError;
            }
        } else {
            // ... or just dump it for debugging
            dump_vhostmsg(&msg.msg);
        }

        if (status == ServerSockAccept) {
            // in_handler will tell us if we need to reply
            if (r > 0) {
                /* Set the version in the flags when sending the reply */
                msg.msg.flags &= ~VHOST_USER_VERSION_MASK;
                msg.msg.flags |= VHOST_USER_VERSION;
                msg.msg.flags |= VHOST_USER_REPLY_MASK;
                // Send data to the other side
                if (vhost_user_send_fds(sock, &msg.msg, 0, 0) < 0) {
                    perror("send");
                    status = ServerSockError;
                }
            }
        }
    }

    return status;
}

/* Accept a connection and add the socket to the fd polling list */
static int accept_sock_server(FdNode* node)
{
    int sock;
    struct sockaddr_un un;
    socklen_t len = sizeof(un);
    ServerSockStatus status = ServerSockInit;
    Server* server = (Server*)node->context;

    // Accept connection on the server socket
    if ((sock = accept(server->sock, (struct sockaddr *) &un, &len)) == -1) {
        perror("accept");
    } else {
        status = ServerSockAccept;
    }

    add_fd_list(&server->fd_list, FD_READ, sock, (void*) server, receive_sock_server);

    // this return value is discarded by process_fd_set
    return status;
}

int loop_server(Server* server)
{
    traverse_fd_list(&server->fd_list);
    if (server->handlers.poll_handler) {
        server->handlers.poll_handler(server->handlers.context);
    }

    return 0;
}

int set_handler_server(Server* server, AppHandlers* handlers)
{
    memcpy(&server->handlers, handlers, sizeof(AppHandlers));

    return 0;
}
