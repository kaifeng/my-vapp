/*
 * vhost_server.c
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fd_list.h"
#include "common.h"
#include "shm.h"
#include "vhost_server.h"
#include "vring.h"

// vhost message handler
typedef int (*MsgHandler)(VhostServer* vhost_server, ServerMsg* msg);

static int avail_handler_server(void* context, void* buf, size_t size);
static uintptr_t map_handler(void* context, uint64_t addr);

extern int app_running;

VhostServer* new_vhost_server(const char* path, int is_listen)
{
    VhostServer* vhost_server = (VhostServer*) calloc(1, sizeof(VhostServer));
    int idx;

    //TODO: handle errors here

    /* alloc and init socket server */
    vhost_server->unsock = new_unsock(path);
    init_server(vhost_server->unsock, is_listen);

    // socket now connected

    /* code below cleans up all VhostServer fields
     * shoudn't codes below placed before the socket op?
     */
    vhost_server->memory.nregions = 0;

    // VringTable initalization
    vhost_server->vring_table.context = (void*) vhost_server;
    vhost_server->vring_table.avail_handler = avail_handler_server;
    vhost_server->vring_table.map_handler = map_handler;

    for (idx = 0; idx < VHOST_CLIENT_VRING_NUM; idx++) {
        vhost_server->vring_table.vring[idx].kickfd = -1;
        vhost_server->vring_table.vring[idx].callfd = -1;
        vhost_server->vring_table.vring[idx].desc = 0;
        vhost_server->vring_table.vring[idx].avail = 0;
        vhost_server->vring_table.vring[idx].used = 0;
        vhost_server->vring_table.vring[idx].num = 0;
        vhost_server->vring_table.vring[idx].last_avail_idx = 0;
        vhost_server->vring_table.vring[idx].last_used_idx = 0;
    }

    vhost_server->buffer_size = 0;
    vhost_server->is_polling = 0;
    init_stat(&vhost_server->stat);    // init time stat struct

    return vhost_server;
}

int end_vhost_server(VhostServer* vhost_server)
{
    int idx;

    // End server
    close_unsock(vhost_server->unsock);
    free(vhost_server->unsock);
    vhost_server->unsock = NULL;

    for (idx = 0; idx < vhost_server->memory.nregions; idx++) {
        VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];
        // shm由client端分配，不要在server端调end_shm
        unmap_shm((void*) (uintptr_t) region->userspace_addr, region->memory_size);
    }

    return 0;
}

static uintptr_t _map_guest_addr(VhostServer* vhost_server, uint64_t addr)
{
    uintptr_t result = 0;
    int idx;

    for (idx = 0; idx < vhost_server->memory.nregions; idx++) {
        VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];

        if (region->guest_phys_addr <= addr
                && addr < (region->guest_phys_addr + region->memory_size)) {
            result = region->mmap_addr + addr - region->guest_phys_addr;
            break;
        }
    }

    return result;
}

static uintptr_t _map_user_addr(VhostServer* vhost_server, uint64_t addr)
{
    uintptr_t result = 0;
    int idx;

    for (idx = 0; idx < vhost_server->memory.nregions; idx++) {
        VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];

        if (region->userspace_addr <= addr
                && addr < (region->userspace_addr + region->memory_size)) {
            result = region->mmap_addr + addr - region->userspace_addr;
            break;
        }
    }

    return result;
}

static int _get_features(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    msg->msg.u64 = 0; // no features
    msg->msg.size = MEMBER_SIZE(VhostUserMsg,u64);

    return 1; // should reply back
}

static int _set_features(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _set_owner(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _reset_owner(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _set_mem_table(VhostServer* vhost_server, ServerMsg* msg)
{
    int idx;
    fprintf(stdout, "%s\n", __FUNCTION__);

    vhost_server->memory.nregions = 0;

    for (idx = 0; idx < msg->msg.memory.nregions; idx++) {
        if (msg->fds[idx] > 0) {
            VhostServerMemoryRegion *region = &vhost_server->memory.regions[idx];

            region->guest_phys_addr = msg->msg.memory.regions[idx].guest_phys_addr;
            region->memory_size = msg->msg.memory.regions[idx].memory_size;
            region->userspace_addr = msg->msg.memory.regions[idx].userspace_addr;

            assert(idx < msg->fd_num);
            assert(msg->fds[idx] > 0);

            region->mmap_addr =
                    (uintptr_t) map_shm(msg->fds[idx], region->memory_size);
            if(region->mmap_addr == 0) {
                LOG("%s: failed to map shared memory\n", __FUNCTION__);
            }
            region->mmap_addr += msg->msg.memory.regions[idx].mmap_offset;

            vhost_server->memory.nregions++;
        }
    }

    fprintf(stdout, "Got memory.nregions %d\n", vhost_server->memory.nregions);

    return 0;
}

static int _set_log_base(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _set_log_fd(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

static int _set_vring_num(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.state.index;

    assert(idx<VHOST_CLIENT_VRING_NUM);

    vhost_server->vring_table.vring[idx].num = msg->msg.state.num;

    return 0;
}

static int _set_vring_addr(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.addr.index;

    assert(idx<VHOST_CLIENT_VRING_NUM);

    vhost_server->vring_table.vring[idx].desc =
            (struct vring_desc*) _map_user_addr(vhost_server,
                    msg->msg.addr.desc_user_addr);
    vhost_server->vring_table.vring[idx].avail =
            (struct vring_avail*) _map_user_addr(vhost_server,
                    msg->msg.addr.avail_user_addr);
    vhost_server->vring_table.vring[idx].used =
            (struct vring_used*) _map_user_addr(vhost_server,
                    msg->msg.addr.used_user_addr);

    vhost_server->vring_table.vring[idx].last_used_idx =
            vhost_server->vring_table.vring[idx].used->idx;

    return 0;
}

static int _set_vring_base(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.state.index;

    assert(idx<VHOST_CLIENT_VRING_NUM);

    vhost_server->vring_table.vring[idx].last_avail_idx = msg->msg.state.num;

    return 0;
}

static int _get_vring_base(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.state.index;

    assert(idx<VHOST_CLIENT_VRING_NUM);

    msg->msg.state.num = vhost_server->vring_table.vring[idx].last_avail_idx;
    msg->msg.size = MEMBER_SIZE(VhostUserMsg,state);

    return 1; // should reply back
}

// vring的_process_desc调用avail_handler，对server端就是该函数
// 它把收到的数据拷到私有的buffer空间，后面poll_server得到buffer_size非零会打出报文
static int avail_handler_server(void* context, void* buf, size_t size)
{
    VhostServer* vhost_server = (VhostServer*) context;

    // copy the packet to our private buffer
    memcpy(vhost_server->buffer, buf, size);
    vhost_server->buffer_size = size;

#ifdef DUMP_PACKETS
    dump_buffer(buf, size);
#endif

    return 0;
}

static uintptr_t map_handler(void* context, uint64_t addr)
{
    VhostServer* vhost_server = (VhostServer*) context;
    return _map_guest_addr(vhost_server, addr);
}

static int _poll_avail_vring(VhostServer* vhost_server, int idx)
{
    uint32_t count = 0;

    // if vring is already set, process the vring
    if (vhost_server->vring_table.vring[idx].desc) {
        count = process_avail_vring(&vhost_server->vring_table, idx);
#ifndef DUMP_PACKETS
        update_stat(&vhost_server->stat, count);
        print_stat(&vhost_server->stat);
#endif
    }

    return count;
}

static int _kick_server(struct fd_node* node)
{
    VhostServer* vhost_server = (VhostServer*) node->context;
    int kickfd = node->fd;
    ssize_t r;
    uint64_t kick_it = 0;

    r = read(kickfd, &kick_it, sizeof(kick_it));

    if (r < 0) {
        perror("recv kick");
    } else if (r == 0) {
        fprintf(stdout, "Kick fd closed\n");
        del_fd_list(&vhost_server->unsock->fd_list, FD_READ, kickfd);
    } else {
#if 0
        fprintf(stdout, "Got kick %"PRId64"\n", kick_it);
#endif
        _poll_avail_vring(vhost_server, VHOST_CLIENT_VRING_IDX_TX);
    }

    return 0;
}

// 在此决定使用中断还是轮询
// server (slave) 监听kick
static int _set_vring_kick(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.u64 & VHOST_USER_VRING_IDX_MASK;
    int validfd = (msg->msg.u64 & VHOST_USER_VRING_NOFD_MASK) == 0;

    assert(idx < VHOST_CLIENT_VRING_NUM);
    if (validfd) {
        assert(msg->fd_num == 1);

        vhost_server->vring_table.vring[idx].kickfd = msg->fds[0];

        fprintf(stdout, "Got kickfd 0x%x\n", vhost_server->vring_table.vring[idx].kickfd);

        if (idx == VHOST_CLIENT_VRING_IDX_TX) {
            add_fd_list(&vhost_server->unsock->fd_list, FD_READ,
                    vhost_server->vring_table.vring[idx].kickfd,
                    (void*) vhost_server, _kick_server);
            fprintf(stdout, "Listening for kicks on 0x%x\n", vhost_server->vring_table.vring[idx].kickfd);
        }
        vhost_server->is_polling = 0;
    } else {
        fprintf(stdout, "Got empty kickfd. Start polling.\n");
        vhost_server->is_polling = 1;
    }
    LOG("%s: is_polling %d\n", __FUNCTION__, vhost_server->is_polling);
    return 0;
}

/* VHOST_USER_SET_VRING_CALL (13) Master payload: u64
   Set the event file descriptor to signal when buffers are used. It
   is passed in the ancillary data.
   Bits (0-7) of the payload contain the vring index. Bit 8 is the
   invalid FD flag. This flag is set when there is no file descriptor
   in the ancillary data. This signals that polling will be used
   instead of waiting for the call.
*/
static int _set_vring_call(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);

    int idx = msg->msg.u64 & VHOST_USER_VRING_IDX_MASK;
    int validfd = (msg->msg.u64 & VHOST_USER_VRING_NOFD_MASK) == 0;

    assert(idx < VHOST_CLIENT_VRING_NUM);
    if (validfd) {
        assert(msg->fd_num == 1);

        vhost_server->vring_table.vring[idx].callfd = msg->fds[0];

        fprintf(stdout, "Got callfd 0x%x\n", vhost_server->vring_table.vring[idx].callfd);
    }

    return 0;
}

static int _set_vring_err(VhostServer* vhost_server, ServerMsg* msg)
{
    fprintf(stdout, "%s\n", __FUNCTION__);
    return 0;
}

// return value > 0 if reply is required. otherwise 0.
// < 0 means error
// TODO: move message handling to a separate module.
static MsgHandler msg_handlers[VHOST_USER_MAX] = {
    0,                  // VHOST_USER_NONE
    _get_features,      // VHOST_USER_GET_FEATURES
    _set_features,      // VHOST_USER_SET_FEATURES
    _set_owner,         // VHOST_USER_SET_OWNER
    _reset_owner,       // VHOST_USER_RESET_OWNER
    _set_mem_table,     // VHOST_USER_SET_MEM_TABLE
    _set_log_base,      // VHOST_USER_SET_LOG_BASE
    _set_log_fd,        // VHOST_USER_SET_LOG_FD
    _set_vring_num,     // VHOST_USER_SET_VRING_NUM
    _set_vring_addr,    // VHOST_USER_SET_VRING_ADDR
    _set_vring_base,    // VHOST_USER_SET_VRING_BASE
    _get_vring_base,    // VHOST_USER_GET_VRING_BASE
    _set_vring_kick,    // VHOST_USER_SET_VRING_KICK
    _set_vring_call,    // VHOST_USER_SET_VRING_CALL
    _set_vring_err,     // VHOST_USER_SET_VRING_ERR
};

// vhost server回调，处理vhost消息，由receive_sock_server调用
static int in_msg_server(void* context, ServerMsg* msg)
{
    VhostServer* vhost_server = (VhostServer*) context;
    int result = 0;

    fprintf(stdout, "Processing message: %s\n", cmd_from_vhost_request(msg->msg.request));

    assert(msg->msg.request > VHOST_USER_NONE && msg->msg.request < VHOST_USER_MAX);

    // call dedicated message handler according to request value.
    if (msg_handlers[msg->msg.request]) {
        result = msg_handlers[msg->msg.request](vhost_server, msg);
    }
    fprintf(stdout, "Processing message: %s Done, result %d\n", cmd_from_vhost_request(msg->msg.request), result);

    return result;
}

static int poll_server(void* context)
{
    VhostServer* vhost_server = (VhostServer*) context;
    int tx_idx = VHOST_CLIENT_VRING_IDX_TX;
    int rx_idx = VHOST_CLIENT_VRING_IDX_RX;
    
    LOG("%s\n", __FUNCTION__);

    if (vhost_server->vring_table.vring[rx_idx].desc) {
        // process TX ring
        if (vhost_server->is_polling) {
            _poll_avail_vring(vhost_server, tx_idx);
        }

        // process RX ring
        if (vhost_server->buffer_size) {
            LOG("%s: buffer_size %d\n", __FUNCTION__, vhost_server->buffer_size);
            // send a packet from the buffer
            /* 注意：server端发送数据时，将数据放在rx ring，而client端是放在tx ring
               可见，tx/rx是针对client，也即master端来说的。
             */
            put_vring(&vhost_server->vring_table, rx_idx,
                      vhost_server->buffer, vhost_server->buffer_size);

            // signal the client
            kick(&vhost_server->vring_table, rx_idx);

            // mark the buffer empty
            vhost_server->buffer_size = 0;
        }
    }

    return 0;
}

static int loop_server(UnSock* unsock)
{
    // 查询socket是否有消息
    int n = traverse_fd_list(&unsock->fd_list);
    LOG("%s: fd count %d\n", __FUNCTION__, n);
    // 查询vring是否有数据
    if (unsock->poll_handler) {
        unsock->poll_handler(unsock->context);
    }

    return 0;
}

int run_vhost_server(VhostServer* vhost_server)
{
    // 设置context和socket消息回调
    vhost_server->unsock->context = vhost_server;
    vhost_server->unsock->in_handler = in_msg_server;
    vhost_server->unsock->poll_handler = poll_server;

    start_stat(&vhost_server->stat);

    app_running = 1; // externally modified
    while (app_running) {
        loop_server(vhost_server->unsock);
    }

    stop_stat(&vhost_server->stat);

    return 0;
}
