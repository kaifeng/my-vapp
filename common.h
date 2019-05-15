/*
 * common.h
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include "vhost_user.h"

#define INSTANCE_CREATED        1
#define INSTANCE_INITIALIZED    2
#define INSTANCE_END            3

#define ONEMEG                  (1024*1024)

#define ETH_PACKET_SIZE         (1518)
#define BUFFER_SIZE             (sizeof(struct virtio_net_hdr) + ETH_PACKET_SIZE)
#define BUFFER_ALIGNMENT        (8)         // alignment in bytes
#define VHOST_SOCK_NAME         "vhost.sock"

// align a value on a boundary
#define ALIGN(v,b)   (((long int)v + (long int)b - 1)&(-(long int)b))

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MEMBER_SIZE(t,m)      (sizeof(((t*)0)->m))

#define DUMP_PACKETS

#define LOG(fmt, ...)                printf(fmt, ##__VA_ARGS__)

struct ServerMsg;

typedef int (*InMsgHandler)(void* context, struct ServerMsg* msg);
typedef int (*PollHandler)(void* context);

typedef struct {
    void* context;      // not used judging by code
    InMsgHandler in_handler;
    PollHandler poll_handler;
} AppHandlers;

struct VhostUserMsg;
enum VhostUserRequest;

const char* cmd_from_vhost_request(VhostUserRequest request);
void dump_vhostmsg(const struct VhostUserMsg* msg);

struct vring_desc;
struct vring_avail;
struct vring_used;
struct vhost_vring;

int vhost_user_send_fds(int fd, const struct VhostUserMsg *msg, int *fds, size_t fd_num);
int vhost_user_recv_fds(int fd, const struct VhostUserMsg *msg, int *fds, size_t *fd_num);

// shared memory interface
extern int shm_fds[];
void* init_shm(const char* path, size_t size, int idx);
void* map_shm_from_fd(int fd, size_t size);
int end_shm(const char* path, void* ptr, size_t size, int idx);

int sync_shm(void* ptr, size_t size);

// debug utilities
void dump_buffer(uint8_t* p, size_t len);
void dump_vring(struct vring_desc* desc, struct vring_avail* avail,struct vring_used* used);
void dump_vhost_vring(struct vhost_vring* vring);

#endif /* COMMON_H_ */
