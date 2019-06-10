/*
 * fd_list.h
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef FD_H_
#define FD_H_

#include <stddef.h>
#include <stdint.h>

#define FD_LIST_SIZE    10

struct fd_node;

typedef int (*fd_handler_t)(struct fd_node* node);

struct fd_node {
    int fd;
    void* context;
    fd_handler_t handler;
};

typedef struct {
    struct fd_node read_fds[FD_LIST_SIZE];
    struct fd_node write_fds[FD_LIST_SIZE];     // 似乎没有使用
    uint32_t ms;     // poll timeout value in ms
} FdList;

// FD_WRITE并未使用
typedef enum {
    FD_READ, FD_WRITE
} FdType;

#define FD_LIST_SELECT_POLL     (0)     // poll and exit
#define FD_LIST_SELECT_5        (200)   // 5 times per sec

int init_fd_list(FdList* fd_list, uint32_t ms);
int add_fd_list(FdList* fd_list, FdType type, int fd, void* context, fd_handler_t handler);
int del_fd_list(FdList* fd_list, FdType type, int fd);
int traverse_fd_list(FdList* fd_list);

#endif /* FD_H_ */
