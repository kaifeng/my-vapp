/*
 * client.c
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

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>

#include "client.h"
#include "common.h"
#include "vhost_user.h"
#include "unsock.h"


// 通过socket发送VhostUser消息
int vhost_ioctl(UnSock* client, VhostUserRequest request, ...)
{
    void *arg;
    va_list ap;

    VhostUserMsg msg;
    struct vhost_vring_file *file = 0;
    int need_reply = 0;
    int fds[VHOST_MEMORY_MAX_NREGIONS];
    size_t fd_num = 0;

    va_start(ap, request);
    arg = va_arg(ap, void *);

    memset(&msg,0,sizeof(VhostUserMsg));
    msg.request = request;
    msg.flags &= ~VHOST_USER_VERSION_MASK;
    msg.flags |= VHOST_USER_VERSION;
    msg.size = 0;

#if 1
    LOG("%s: Send message %s\n", __FUNCTION__, cmd_from_vhost_request(request));
#endif

    switch (request) {
    case VHOST_USER_GET_FEATURES:
    case VHOST_USER_GET_VRING_BASE:
        need_reply = 1;
        break;

    case VHOST_USER_SET_FEATURES:
    case VHOST_USER_SET_LOG_BASE:
        msg.u64 = *((uint64_t*) arg);
        msg.size = MEMBER_SIZE(VhostUserMsg,u64);
        break;

    case VHOST_USER_SET_OWNER:
    case VHOST_USER_RESET_OWNER:
        break;

    case VHOST_USER_SET_MEM_TABLE:
        memcpy(&msg.memory, arg, sizeof(VhostUserMemory));
        msg.size = MEMBER_SIZE(VhostUserMemory,nregions);
        msg.size += MEMBER_SIZE(VhostUserMemory,padding);
        for(;fd_num < msg.memory.nregions;fd_num++) {
            fds[fd_num] = shm_fds[fd_num];
            msg.size += sizeof(VhostUserMemoryRegion);
        }
        break;

    case VHOST_USER_SET_LOG_FD:
        fds[fd_num++] = *((int*) arg);
        break;

    case VHOST_USER_SET_VRING_NUM:
    case VHOST_USER_SET_VRING_BASE:
        memcpy(&msg.state, arg, MEMBER_SIZE(VhostUserMsg,state));
        msg.size = MEMBER_SIZE(VhostUserMsg,state);
        break;

    case VHOST_USER_SET_VRING_ADDR:
        memcpy(&msg.addr, arg, MEMBER_SIZE(VhostUserMsg,addr));
        msg.size = MEMBER_SIZE(VhostUserMsg,addr);
        break;

    case VHOST_USER_SET_VRING_KICK:
    case VHOST_USER_SET_VRING_CALL:
    case VHOST_USER_SET_VRING_ERR:
        file = arg;
        msg.u64 = file->index;
        msg.size = MEMBER_SIZE(VhostUserMsg,u64);
        if (file->fd > 0) {
            fds[fd_num++] = file->fd;
        }
        break;

    case VHOST_USER_NONE:
        break;
    default:
        return -1;
    }


    if (vhost_user_send_fds(client->sock, &msg, fds, fd_num) < 0) {
        fprintf(stderr, "ioctl send\n");
        return -1;
    }

    if (need_reply) {

        msg.request = VHOST_USER_NONE;
        msg.flags = 0;

        if (vhost_user_recv_fds(client->sock, &msg, fds, &fd_num) < 0) {
            fprintf(stderr, "ioctl rcv failed\n");
            return -1;
        }

        assert((msg.request == request));
        assert(((msg.flags & VHOST_USER_VERSION_MASK) == VHOST_USER_VERSION));

        switch (request) {
        case VHOST_USER_GET_FEATURES:
            *((uint64_t*) arg) = msg.u64;
            break;
        case VHOST_USER_GET_VRING_BASE:
            memcpy(arg, &msg.state, sizeof(struct vhost_vring_state));
            break;
        default:
            return -1;
        }

    }

    va_end(ap);

    return 0;
}
