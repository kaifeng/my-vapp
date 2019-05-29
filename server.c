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

static int receive_sock_server(struct fd_node* node);
static int accept_sock_server(struct fd_node* node);

/* initialize socket, start service or connect.
 * on sucess, socket is connected
 */
int init_server(UnSock* server, int is_listen)
{
    struct sockaddr_un un;
    size_t len;

    if (server->sock_path == NULL) {
        perror("server: sock path is empty");
        return 0;
    }

    // Create the socket
    if ((server->sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, server->sock_path);

    len = sizeof(un.sun_family) + strlen(server->sock_path);

    if (is_listen) {
        unlink(server->sock_path); // remove if exists

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

    server->is_connected = 1;
    server->is_server = is_listen;

    return 0;
}

/* there is data coming in from socket
 * further logic continues at common.c vhost_user_recv_fds and vhost_user_send_fds
 */
static int receive_sock_server(struct fd_node* node)
{
    UnSock* server = (UnSock*) node->context;
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
    } else {
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
        } else {
            // ... or just dump it for debugging
            dump_vhostmsg(&msg.msg);
        }
    }

    return status;
}

/* Accept a connection and add the socket to the fd polling list */
static int accept_sock_server(struct fd_node* node)
{
    int sock;
    struct sockaddr_un un;
    socklen_t len = sizeof(un);
    ServerSockStatus status = ServerSockInit;
    UnSock* server = (UnSock*)node->context;

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

int loop_server(UnSock* server)
{
    traverse_fd_list(&server->fd_list);
    if (server->handlers.poll_handler) {
        server->handlers.poll_handler(server->handlers.context);
    }

    return 0;
}

int set_handler_server(UnSock* server, AppHandlers* handlers)
{
    memcpy(&server->handlers, handlers, sizeof(AppHandlers));

    return 0;
}
