/*
 * fd_list.c
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include <stdio.h>
#include <sys/select.h>

#include "common.h"
#include "fd_list.h"

static int reset_fd_node(struct fd_node* fd_node)
{
    fd_node->fd = -1;
    fd_node->context = NULL;
    fd_node->handler = NULL;

    return 0;
}

int init_fd_list(FdList* fd_list, uint32_t ms)
{
    int idx;

    for (idx = 0; idx < FD_LIST_SIZE; idx++) {
        reset_fd_node(&(fd_list->read_fds[idx]));
        reset_fd_node(&(fd_list->write_fds[idx]));
    }

    fd_list-> ms = ms;

    return 0;
}

static struct fd_node* find_fd_node(struct fd_node* fds, int fd)
{
    int idx;

    for (idx = 0; idx < FD_LIST_SIZE; idx++) {
        if (fds[idx].fd == fd) {
            return &(fds[idx]);
        }
    }

    return NULL;
}

/* find an unsed fd_node and add specified fd/context/handler to the list */
int add_fd_list(FdList* fd_list, FdType type, int fd, void* context, fd_handler_t handler)
{
    struct fd_node* fds = (type == FD_READ) ? fd_list->read_fds : fd_list->write_fds;
    struct fd_node* fd_node = find_fd_node(fds, -1);

    if (!fd_node) {
        perror("No space in fd list");
        return -1;
    }

    fd_node->fd = fd;
    fd_node->context = context;
    fd_node->handler = handler;

    return 0;
}

int del_fd_list(FdList* fd_list, FdType type, int fd)
{
    struct fd_node* fds = (type == FD_READ) ? fd_list->read_fds : fd_list->write_fds;
    struct fd_node* fd_node = find_fd_node(fds, fd);

    if (!fd_node) {
        fprintf(stderr, "Fd (%d) not found fd list\n", fd);
        return -1;
    }

    reset_fd_node(fd_node);

    return 0;
}

/* 生成一个文件描述符集合，宏由C库函数提供，
   集合包括目前有效的文件描述符（非-1），
   fdmax是现有集合里文件描述符的最大值。
   返回值：集合内fd最大值
*/
static int get_fd_set(FdList* fd_list, FdType type, fd_set* fdset)
{
    int idx;
    struct fd_node* fds = (type == FD_READ) ? fd_list->read_fds : fd_list->write_fds;

    FD_ZERO(fdset);

    int fdmax = -1;
    for (idx = 0; idx < FD_LIST_SIZE; idx++) {
        int fd = fds[idx].fd;
        if (fd != -1) {
            FD_SET(fd, fdset);
            fdmax = MAX(fd, fdmax);
        }
    }

    return fdmax;
}

/* 针对fd集合，调用回调函数，handler为下列一种：
   _kick_client
   _kick_server
   accept_sock_server
   receive_sock_server
*/
static int process_fd_set(FdList* fd_list, FdType type, fd_set* fdset)
{
    int idx;
    int num_of_fds = 0;     // introduced fix
    struct fd_node* fds = (type == FD_READ) ? fd_list->read_fds : fd_list->write_fds;

    for (idx = 0; idx < FD_LIST_SIZE; idx++) {
        struct fd_node* node = &(fds[idx]);
        if (FD_ISSET(node->fd,fdset)) {
            num_of_fds++;
            if (node->handler) {
                node->handler(node);
            }
        }
    }

    // return 0;
    return num_of_fds;
}

int traverse_fd_list(FdList* fd_list)
{
    fd_set read_fdset, write_fdset;
    struct timeval tv = { .tv_sec = fd_list->ms/1000,
                          .tv_usec = (fd_list->ms%1000)*1000 };
    int r;

    int rfd_max = get_fd_set(fd_list, FD_READ, &read_fdset);
    int wfd_max = get_fd_set(fd_list, FD_WRITE, &write_fdset);
    // man: nfds should be set to the highest-numbered file descriptor in any of the three sets, plus 1.
    int nfds = MAX(rfd_max, wfd_max) + 1;
    
    r = select(nfds, &read_fdset, &write_fdset, 0, &tv);

    if (r == -1) {
        perror("select");
    } else if (r == 0) {
        // no ready fds, timeout
    } else {
        // non-zero, something available
        // check accept_sock_server (listen) or receive_sock_server (connect) for further
        // processing logic
        int rr = process_fd_set(fd_list, FD_READ, &read_fdset);
        int wr = process_fd_set(fd_list, FD_WRITE, &write_fdset);
        if (r != (rr + wr)) {
            // something wrong
        }
    }

    return 0;
}
