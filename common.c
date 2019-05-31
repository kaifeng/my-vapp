/*
 * common.c
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/unistd.h>

#include "common.h"
#include "vring.h"
#include "vhost_user.h"
#include "vhost.h"
#include "shm.h"
#include "fd_list.h"
#include "unsock.h"



/*
 * Common code for UNIX domain socket messaging
 */

// send a vhost user message to the other side via socket message,
// fds is put into ancillary data if provided.
int vhost_user_send_fds(int fd, const VhostUserMsg *msg, int *fds,
        size_t fd_num)
{
    int ret;

    struct msghdr msgh;     // a socket struct, describing the message.
    struct iovec iov[1];

    size_t fd_size = fd_num * sizeof(int);
    char control[CMSG_SPACE(fd_size)];
    struct cmsghdr *cmsg;       // ancillary data for this socket.

    memset(&msgh, 0, sizeof(msgh));
    memset(control, 0, sizeof(control));

    /* set the payload */
    iov[0].iov_base = (void *) msg;
    iov[0].iov_len = VHOST_USER_HDR_SIZE + msg->size;

    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;

    if (fd_num) {
        msgh.msg_control = control;
        msgh.msg_controllen = sizeof(control);

        cmsg = CMSG_FIRSTHDR(&msgh);

        cmsg->cmsg_len = CMSG_LEN(fd_size);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        memcpy(CMSG_DATA(cmsg), fds, fd_size);
    }

    do {
        ret = sendmsg(fd, &msgh, 0);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        fprintf(stderr, "Failed to send msg, reason: %s\n", strerror(errno));
    }

    return ret;
}

// receive a vhost user message from the other side via socket message,
// ancillary data is populated into fds if exists.
int vhost_user_recv_fds(int fd, const VhostUserMsg *msg, int *fds,
        size_t *fd_num)
{
    int ret;

    struct msghdr msgh;
    struct iovec iov[1];

    size_t fd_size = (*fd_num) * sizeof(int);
    char control[CMSG_SPACE(fd_size)];
    struct cmsghdr *cmsg;

    memset(&msgh, 0, sizeof(msgh));
    memset(control, 0, sizeof(control));
    *fd_num = 0;

    /* set the payload */
    iov[0].iov_base = (void *) msg;
    iov[0].iov_len = VHOST_USER_HDR_SIZE;

    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = control;
    msgh.msg_controllen = sizeof(control);

    ret = recvmsg(fd, &msgh, 0);        // return num of bytes received, in this case, the header size.
    if (ret > 0) {
        if (msgh.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
            ret = -1;
        } else {
            cmsg = CMSG_FIRSTHDR(&msgh);
            // handle control data
            if (cmsg && cmsg->cmsg_len > 0 &&
                cmsg->cmsg_level == SOL_SOCKET &&
                cmsg->cmsg_type == SCM_RIGHTS) {
                if (fd_size >= cmsg->cmsg_len - CMSG_LEN(0)) {
                    fd_size = cmsg->cmsg_len - CMSG_LEN(0);
                    memcpy(fds, CMSG_DATA(cmsg), fd_size);
                    *fd_num = fd_size / sizeof(int);
                }
            }
        }
    }

    if (ret < 0) {
        fprintf(stderr, "Failed recvmsg, reason: %s\n", strerror(errno));
    } else {
        // further reading the message body
        read(fd, ((char*)msg) + ret, msg->size);
    }

    return ret;
}

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
