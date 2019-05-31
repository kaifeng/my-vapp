#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>

#include "common.h"
#include "unsock.h"

/* alloc a new socket struct, initialize state to INSTANCE_CREATED */
UnSock* new_unsock(const char* path)
{
    UnSock* s = (UnSock*) calloc(1, sizeof(UnSock));
    strncpy(s->sock_path, path ? path : VHOST_SOCK_NAME, PATH_MAX);
    return s;
}

/* close socket */
int close_unsock(UnSock* s)
{
    if (s->is_connected) {
        // Close and unlink the socket
        close(s->sock);
        s->is_connected = 0;
        if(s->is_server) {
            unlink(s->sock_path);
        }
    }

    return 0;
}

/* 创建unix domain socket并连接到目的端，初始化fd list */
int init_client(UnSock* unsock)
{
    struct sockaddr_un un;
    size_t len;

    if (unsock->sock_path == NULL) {
        perror("unsock: sock_path is empty");
        return 0;
    }

    // Create the socket
    if ((unsock->sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, unsock->sock_path);

    len = sizeof(un.sun_family) + strlen(unsock->sock_path);    // why not sizeof(un)?

    // Connect
    if (connect(unsock->sock, (struct sockaddr *) &un, len) == -1) {
        perror("connect");
        return -1;
    }

    unsock->is_server = 0;
    unsock->is_connected = 1;

    init_fd_list(&unsock->fd_list, FD_LIST_SELECT_POLL);

    return 0;
}

static int receive_sock_server(struct fd_node* node);
static int accept_sock_server(struct fd_node* node);

/* initialize socket, start service or connect.
 * on sucess, socket is connected
 */
int init_server(UnSock* unsock, int is_listen)
{
    struct sockaddr_un un;
    size_t len;

    if (unsock->sock_path == NULL) {
        perror("server: sock path is empty");
        return 0;
    }

    // Create the socket
    if ((unsock->sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, unsock->sock_path);

    len = sizeof(un.sun_family) + strlen(unsock->sock_path);

    if (is_listen) {
        unlink(unsock->sock_path); // remove if exists

        // Bind
        if (bind(unsock->sock, (struct sockaddr *) &un, len) == -1) {
            perror("bind");
            return -1;
        }

        // Listen
        if (listen(unsock->sock, 1) == -1) {
            perror("listen");
            return -1;
        }
    } else {
        if (connect(unsock->sock, (struct sockaddr *) &un, len) == -1) {
            perror("connect");
            return -1;
        }
    }

    /* init a fd_list struct, reset all fds and handler */
    init_fd_list(&unsock->fd_list, FD_LIST_SELECT_5);
    /* pick an unused fd node (fd = -1) and add sock as well as handler to it.
     * if the server is listening, read means a connection is coming in,
     * otherwise, it means a buffer is comming in.
     */
    add_fd_list(&unsock->fd_list, FD_READ, unsock->sock, (void*) unsock,
            is_listen?accept_sock_server:receive_sock_server);

    unsock->is_connected = 1;
    unsock->is_server = is_listen;

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
    int status = 0;
    int r;

    msg.fd_num = sizeof(msg.fds)/sizeof(int);

    // Receive data from the other side
    r = vhost_user_recv_fds(sock, &msg.msg, msg.fds, &msg.fd_num);
    if (r < 0) {
        perror("recv");
        return -1;
    }
    
    if (r == 0) {
        del_fd_list(&server->fd_list, FD_READ, sock);
        close(sock);
        LOG("connection closed\n");
        return 0;
    }

#ifdef DUMP_PACKETS
    dump_vhostmsg(&msg.msg);
#endif
    r = 0;
    // Handle the packet to the registered server backend
    // see vhost_server in_msg_server()
    if (server->in_handler) {
        void* ctx = server->context;
        r = server->in_handler(ctx, &msg);
        if (r < 0) {
            fprintf(stderr, "Error processing message: %s\n",
                    cmd_from_vhost_request(msg.msg.request));
            status = -1;
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
                status = -1;
            }
        }
    } else {
        LOG("No available recv handler, message not processed.\n");
        // ... or just dump it for debugging
        dump_vhostmsg(&msg.msg);
    }

    return status;
}

/* Accept a connection and add the socket to the fd polling list */
static int accept_sock_server(struct fd_node* node)
{
    int sock;
    struct sockaddr_un un;
    socklen_t len = sizeof(un);
    UnSock* server = (UnSock*)node->context;

    // Accept connection on the server socket
    if ((sock = accept(server->sock, (struct sockaddr *) &un, &len)) == -1) {
        perror("accept connection");
        return -1;
    }

    add_fd_list(&server->fd_list, FD_READ, sock, (void*) server, receive_sock_server);

    // this return value is discarded by process_fd_set
    return 0;
}
