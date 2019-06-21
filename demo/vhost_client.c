/*
 * vhost_client.c
 *
 * Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "packet.h"
#include "common.h"
#include "shm.h"
#include "stat.h"
#include "vhost_client.h"
#include "unsock.h"


#define VHOST_CLIENT_TEST_MESSAGE        (arp_request)
#define VHOST_CLIENT_TEST_MESSAGE_LEN    (sizeof(arp_request))
#define VHOST_CLIENT_PAGE_SIZE \
            ALIGN(sizeof(struct vhost_vring)+BUFFER_SIZE*VHOST_VRING_SIZE, ONEMEG)

static int _kick_client(struct fd_node* node);
static int avail_handler_client(void* context, void* buf, size_t size);


VhostClient* new_vhost_client(const char* path)
{
    VhostClient* vhost_client = (VhostClient*) calloc(1, sizeof(VhostClient));
    int idx = 0;

    // create unsock and connect
    vhost_client->unsock = new_unsock(path);
    
    // 创建共享内存regions，数量与VRING数量相同
    vhost_client->page_size = VHOST_CLIENT_PAGE_SIZE;
    vhost_client->memory.nregions = VHOST_CLIENT_VRING_NUM;
    for (idx = 0; idx < vhost_client->memory.nregions; idx++) {
        void* shm = create_shm(vhost_client->page_size, idx);
        if (!shm) {
            fprintf(stderr, "Creating shm %d failed\n", idx);
            free(vhost_client->unsock);
            free(vhost_client);
            return NULL;
        }
        vhost_client->memory.regions[idx].guest_phys_addr = (uintptr_t) shm;
        vhost_client->memory.regions[idx].memory_size = vhost_client->page_size;
        vhost_client->memory.regions[idx].userspace_addr = (uintptr_t) shm;
        vhost_client->memory.regions[idx].mmap_offset = 0;
    }

    // 在memory初始化vring结构，并把指针赋给vring_table_shm，vring_table_shm会作为MEM_TABLE发给对端。
    /* TODO: here we assume we're putting each vring in a separate
     * memory region from the memory map.
     * In reality this probably is not like that
     */
    for (idx = 0; idx < VHOST_CLIENT_VRING_NUM; idx++) {
        struct vhost_vring* vring = new_vring((void*)(uintptr_t)vhost_client->memory.regions[idx].guest_phys_addr);
        if (!vring) {
            fprintf(stderr, "Unable to create vring from memory region %d.\n", idx);
            return NULL;
        }
        vhost_client->vring_table_shm[idx] = vring;
    }

    return vhost_client;
}

// client的初始化流程，与server的消息交互在此
// 把vring发给server，并初始化自身的VringTable结构
int init_vhost_client(VhostClient* vhost_client)
{
    int idx;

    if (!vhost_client->unsock) {
        return -1;
    }

    // 初始化socket client并建立连接
    if (init_unsock(vhost_client->unsock, 0, FD_LIST_SELECT_POLL, NULL) != 0) {
        return -1;
    }

    /* VHOST_USER_SET_OWNER (3)
       Issued when a new connection is established. It sets the current Master
       as an owner of the session. This can be used on the Slave as a
       "session start" flag.
    */
    vhost_ioctl(vhost_client->unsock, VHOST_USER_SET_OWNER, 0);

    /* VHOST_USER_GET_FEATURES (1)
       Get from the underlying vhost implementation the features bitmask.
       Feature bit VHOST_USER_F_PROTOCOL_FEATURES signals slave support for
       VHOST_USER_GET_PROTOCOL_FEATURES and VHOST_USER_SET_PROTOCOL_FEATURES.
       Slave payload: u64
    */
    vhost_ioctl(vhost_client->unsock, VHOST_USER_GET_FEATURES, &vhost_client->features);

    /* VHOST_USER_SET_MEM_TABLE (5)
       Sets the memory map regions on the slave so it can translate the vring
       addresses. In the ancillary data there is an array of file descriptors
       for each memory mapped region. The size and ordering of the fds matches
       the number and ordering of memory regions.
     */
    vhost_ioctl(vhost_client->unsock, VHOST_USER_SET_MEM_TABLE, &vhost_client->memory);

    // push the vring table info to the server
    // 2个vring，一个收，一个发
    if (set_host_vring_table(vhost_client->vring_table_shm, VHOST_CLIENT_VRING_NUM,
            vhost_client->unsock) != 0) {
        // TODO: handle error here
    }

    // VringTable initalization
    vhost_client->vring_table.context = (void*) vhost_client;
    vhost_client->vring_table.avail_handler = avail_handler_client;
    vhost_client->vring_table.map_handler = NULL;

    for (idx = 0; idx < VHOST_CLIENT_VRING_NUM; idx++) {
        vhost_client->vring_table.vring[idx].kickfd = vhost_client->vring_table_shm[idx]->kickfd;
        vhost_client->vring_table.vring[idx].callfd = vhost_client->vring_table_shm[idx]->callfd;
        vhost_client->vring_table.vring[idx].desc = vhost_client->vring_table_shm[idx]->desc;
        vhost_client->vring_table.vring[idx].avail = &vhost_client->vring_table_shm[idx]->avail;
        vhost_client->vring_table.vring[idx].used = &vhost_client->vring_table_shm[idx]->used;
        vhost_client->vring_table.vring[idx].num = VHOST_VRING_SIZE;
        vhost_client->vring_table.vring[idx].last_avail_idx = 0;
        vhost_client->vring_table.vring[idx].last_used_idx = 0;
    }

    // Add handler for RX kickfd
    add_fd_list(&vhost_client->unsock->fd_list, FD_READ,
            vhost_client->vring_table.vring[VHOST_CLIENT_VRING_IDX_RX].kickfd,
            (void*) vhost_client, _kick_client);

    return 0;
}

int end_vhost_client(VhostClient* vhost_client)
{
    int i = 0;

    vhost_ioctl(vhost_client->unsock, VHOST_USER_RESET_OWNER, 0);

    // free all shared memory mappings
    for (i = 0; i<vhost_client->memory.nregions; i++)
    {
        end_shm((void*) (uintptr_t) vhost_client->memory.regions[i].guest_phys_addr,
                vhost_client->memory.regions[i].memory_size, i);
    }

    close_unsock(vhost_client->unsock);

    //TODO: should this be here?
    free(vhost_client->unsock);
    vhost_client->unsock = NULL;

    return 0;
}

static int send_packet(VhostClient* vhost_client, void* p, size_t size)
{
    int r = 0;
    uint32_t tx_idx = VHOST_CLIENT_VRING_IDX_TX;

    r = put_vring(&vhost_client->vring_table, tx_idx, p, size);

    if (r != 0)
        return -1;

    return kick(&vhost_client->vring_table, tx_idx);
}

static int avail_handler_client(void* context, void* buf, size_t size)
{
    // consume the packet
#if 0
    dump_buffer(buf, size);
#endif

    return 0;
}

static int _kick_client(struct fd_node* node)
{
    VhostClient* vhost_client = (VhostClient*) node->context;
    int kickfd = node->fd;
    ssize_t r;
    uint64_t kick_it = 0;

    r = read(kickfd, &kick_it, sizeof(kick_it));

    if (r < 0) {
        perror("recv kick");
    } else if (r == 0) {
        fprintf(stdout, "Kick fd closed\n");
        del_fd_list(&vhost_client->unsock->fd_list, FD_READ, kickfd);
    } else {
        int idx = VHOST_CLIENT_VRING_IDX_RX;
#if 0
        fprintf(stdout, "Got kick %ld\n", kick_it);
#endif

        process_avail_vring(&vhost_client->vring_table, idx);
    }

    return 0;
}

static int poll_client(void* context)
{
    VhostClient* vhost_client = (VhostClient*) context;
    uint32_t tx_idx = VHOST_CLIENT_VRING_IDX_TX;

    LOG("%s: process_used_vring\n", __FUNCTION__);
    if (process_used_vring(&vhost_client->vring_table, tx_idx) != 0) {
        fprintf(stderr, "handle_used_vring failed.\n");
        return -1;
    }

    LOG("%s: send_packet\n", __FUNCTION__);
    if (send_packet(vhost_client, (void*) VHOST_CLIENT_TEST_MESSAGE,
            VHOST_CLIENT_TEST_MESSAGE_LEN) != 0) {
        fprintf(stdout, "Send packet failed.\n");
        return -1;
    }

    update_stat(&vhost_client->stat,1);
    print_stat(&vhost_client->stat);

    return 0;
}

extern int app_running;

int loop_client(UnSock* unsock)
{
    // externally modified
    app_running = 1;

    while (app_running) {
        // 查询socket是否有数据
        int n = traverse_fd_list(&unsock->fd_list);
        LOG("%s: fd count %d\n", __FUNCTION__, n);
        // 查询vring是否有数据
        if (unsock->poll_handler) {
            unsock->poll_handler(unsock->context);
        }
#ifdef DUMP_PACKETS
        sleep(1);
#endif
    }

    return 0;
}

int run_vhost_client(VhostClient* vhost_client)
{
    if (init_vhost_client(vhost_client) != 0)
        return -1;

    // 设置context和socket消息回调，client侧只设置了poll回调，也即不收socket消息
    vhost_client->unsock->context = vhost_client;
    vhost_client->unsock->in_handler = NULL;
    vhost_client->unsock->poll_handler = poll_client;

    start_stat(&vhost_client->stat);
    loop_client(vhost_client->unsock);
    stop_stat(&vhost_client->stat);

    end_vhost_client(vhost_client);

    return 0;
}

/* CODES FOR RUNNING VHOST CLIENT */

static struct sigaction sigact;
int app_running = 0;

static void signal_handler(int);
static void init_signals(void);
static void cleanup(void);

int main(int argc, char* argv[])
{
    VhostClient *vhost_master = NULL;

    atexit(cleanup);
    init_signals();

    char *path = argc == 2 ? argv[1] : NULL;

    /* vhost-user client, can be qemu */
    vhost_master = new_vhost_client(path);
    run_vhost_client(vhost_master);
    free(vhost_master);

    return EXIT_SUCCESS;

}

static void signal_handler(int sig){
    switch(sig)
    {
    case SIGINT:
    case SIGKILL:
    case SIGTERM:
        app_running = 0;
        break;
    default:
        break;
    }
}

static void init_signals(void){
    sigact.sa_handler = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, (struct sigaction *)NULL);
}

static void cleanup(void){
    sigemptyset(&sigact.sa_mask);
    app_running = 0;
}
