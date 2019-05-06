/*
 * common.c
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <sys/mman.h>

#include "common.h"
#include "vring.h"
#include "vhost_user.h"
#include "vhost.h"

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
    } else {    // not required, already zeroed at L35.
        msgh.msg_control = 0;
        msgh.msg_controllen = 0;
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

/*
 * Common code for shared memory
 */

int shm_fds[VHOST_MEMORY_MAX_NREGIONS];

/* 创建一个RW的共享内存 */
void* init_shm(const char* path, size_t size, int idx)
{
    int fd = 0;
    void* result = 0;
    char path_idx[PATH_MAX];
    int oflags = 0;

    sprintf(path_idx, "%s%d", path, idx);

    oflags = O_RDWR | O_CREAT;

    fd = shm_open(path_idx, oflags, 0666);
    if (fd == -1) {
        perror("shm_open");
        goto err;
    }

    if (ftruncate(fd, size) != 0) {
        perror("ftruncate");
        goto err;
    }

    result = init_shm_from_fd(fd, size);
    if (!result) {
        goto err;
    }

    shm_fds[idx] = fd;

    return result;

err:
    close(fd);
    return 0;
}

/* 映身共享内存 */
void* init_shm_from_fd(int fd, size_t size) {
    void *result = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (result == MAP_FAILED) {
        perror("mmap");
        result = 0;
    }
    return result;
}

/* 取消共享内存映射并删除共享内存 */
int end_shm(const char* path, void* ptr, size_t size, int idx)
{
    char path_idx[PATH_MAX];

    if (shm_fds[idx] > 0) {
        close(shm_fds[idx]);
    }

    sprintf(path_idx, "%s%d", path, idx);

    if (munmap(ptr, size) != 0) {
        perror("munmap");
        return -1;
    }

    if (shm_unlink(path_idx) != 0) {
        perror("shm_unlink");
        return -1;
    }

    return 0;
}

/* synchronize a file with a memory map */
int sync_shm(void* ptr, size_t size)
{
    return msync(ptr, size, MS_SYNC | MS_INVALIDATE);
}