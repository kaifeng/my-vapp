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

#include <limits.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include "vhost_user.h"
#include "fd_list.h"
#include "unsock.h"

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

struct VhostUserMsg;
enum VhostUserRequest;

const char* cmd_from_vhost_request(VhostUserRequest request);
void dump_vhostmsg(const struct VhostUserMsg* msg);

struct vring_desc;
struct vring_avail;
struct vring_used;
struct vhost_vring;

// vhost_user interface
int vhost_ioctl(UnSock* client, enum VhostUserRequest request, ...);
int vhost_user_send_fds(int fd, const struct VhostUserMsg *msg, int *fds, size_t fd_num);
int vhost_user_recv_fds(int fd, const struct VhostUserMsg *msg, int *fds, size_t *fd_num);

// debug utilities
void dump_buffer(uint8_t* p, size_t len);
void dump_vring(struct vring_desc* desc, struct vring_avail* avail,struct vring_used* used);
void dump_vhost_vring(struct vhost_vring* vring);

#endif /* COMMON_H_ */
