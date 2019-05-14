#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "common.h"
#include "vring.h"
#include "vhost_user.h"

// Convert message request type to human readable string
const char* cmd_from_vhost_request(VhostUserRequest request)
{
    switch (request) {
    case VHOST_USER_NONE:
        return "VHOST_USER_NONE";
    case VHOST_USER_GET_FEATURES:
        return "VHOST_USER_GET_FEATURES";
    case VHOST_USER_SET_FEATURES:
        return "VHOST_USER_SET_FEATURES";
    case VHOST_USER_SET_OWNER:
        return "VHOST_USER_SET_OWNER";
    case VHOST_USER_RESET_OWNER:
        return "VHOST_USER_RESET_OWNER";
    case VHOST_USER_SET_MEM_TABLE:
        return "VHOST_USER_SET_MEM_TABLE";
    case VHOST_USER_SET_LOG_BASE:
        return "VHOST_USER_SET_LOG_BASE";
    case VHOST_USER_SET_LOG_FD:
        return "VHOST_USER_SET_LOG_FD";
    case VHOST_USER_SET_VRING_NUM:
        return "VHOST_USER_SET_VRING_NUM";
    case VHOST_USER_SET_VRING_ADDR:
        return "VHOST_USER_SET_VRING_ADDR";
    case VHOST_USER_SET_VRING_BASE:
        return "VHOST_USER_SET_VRING_BASE";
    case VHOST_USER_GET_VRING_BASE:
        return "VHOST_USER_GET_VRING_BASE";
    case VHOST_USER_SET_VRING_KICK:
        return "VHOST_USER_SET_VRING_KICK";
    case VHOST_USER_SET_VRING_CALL:
        return "VHOST_USER_SET_VRING_CALL";
    case VHOST_USER_SET_VRING_ERR:
        return "VHOST_USER_SET_VRING_ERR";
    case VHOST_USER_MAX:
        return "VHOST_USER_MAX";
    }

    return "UNDEFINED";
}

// Dump a user message
void dump_vhostmsg(const VhostUserMsg* msg)
{
    int i = 0;
    fprintf(stdout, "......dump vhost message start......\n");
    fprintf(stdout, "Cmd: %s (0x%x)\n", cmd_from_vhost_request(msg->request), msg->request);
    fprintf(stdout, "Flags: 0x%x\n", msg->flags);

    // command specific `dumps`
    switch (msg->request) {
    case VHOST_USER_GET_FEATURES:
        fprintf(stdout, "u64: 0x%"PRIx64"\n", msg->u64);
        break;
    case VHOST_USER_SET_FEATURES:
        fprintf(stdout, "u64: 0x%"PRIx64"\n", msg->u64);
        break;
    case VHOST_USER_SET_OWNER:
        break;
    case VHOST_USER_RESET_OWNER:
        break;
    case VHOST_USER_SET_MEM_TABLE:
        fprintf(stdout, "nregions: %d\n", msg->memory.nregions);
        for (i = 0; i < msg->memory.nregions; i++) {
            fprintf(stdout,
                    "region: \n\tgpa = 0x%"PRIX64"\n\tsize = %"PRId64"\n\tua = 0x%"PRIx64"\n",
                    msg->memory.regions[i].guest_phys_addr,
                    msg->memory.regions[i].memory_size,
                    msg->memory.regions[i].userspace_addr);
        }
        break;
    case VHOST_USER_SET_LOG_BASE:
        fprintf(stdout, "u64: 0x%"PRIx64"\n", msg->u64);
        break;
    case VHOST_USER_SET_LOG_FD:
        break;
    case VHOST_USER_SET_VRING_NUM:
        fprintf(stdout, "state: %d %d\n", msg->state.index, msg->state.num);
        break;
    case VHOST_USER_SET_VRING_ADDR:
        fprintf(stdout, "addr:\n\tidx = %d\n\tflags = 0x%x\n"
                "\tdua = 0x%"PRIx64"\n"
                "\tuua = 0x%"PRIx64"\n"
                "\taua = 0x%"PRIx64"\n"
                "\tlga = 0x%"PRIx64"\n", msg->addr.index, msg->addr.flags,
                msg->addr.desc_user_addr, msg->addr.used_user_addr,
                msg->addr.avail_user_addr, msg->addr.log_guest_addr);
        break;
    case VHOST_USER_SET_VRING_BASE:
        fprintf(stdout, "state: %d %d\n", msg->state.index, msg->state.num);
        break;
    case VHOST_USER_GET_VRING_BASE:
        fprintf(stdout, "state: %d %d\n", msg->state.index, msg->state.num);
        break;
    case VHOST_USER_SET_VRING_KICK:
    case VHOST_USER_SET_VRING_CALL:
    case VHOST_USER_SET_VRING_ERR:
        fprintf(stdout, "u64: 0x%"PRIx64"\n", msg->u64);
        break;
    case VHOST_USER_NONE:
    case VHOST_USER_MAX:
        break;
    }

    fprintf(stdout, "......dump vhost message end......\n");
}

// dump a buffer in a hexdump way
void dump_buffer(uint8_t* p, size_t len)
{
    int i;
    fprintf(stdout, "......dump buffer start......\n");
    for(i=0;i<len;i++) {
        if(i%16 == 0)fprintf(stdout,"\n");
        fprintf(stdout,"%.2x ",p[i]);
    }
    fprintf(stdout, "......dump buffer end......\n");
}

// dump a vring struct
void dump_vring(struct vring_desc* desc, struct vring_avail* avail,struct vring_used* used)
{
    int idx;

    fprintf(stdout,"desc:\n");
    for(idx=0;idx<VHOST_VRING_SIZE;idx++){
        fprintf(stdout, "%d: 0x%"PRIx64" %d 0x%x %d\n",
                idx,
                desc[idx].addr, desc[idx].len,
                desc[idx].flags, desc[idx].next);
    }

    fprintf(stdout,"avail:\n");
    for(idx=0;idx<VHOST_VRING_SIZE;idx++){
       int desc_idx = avail->ring[idx];
       fprintf(stdout, "%d: %d\n",idx, desc_idx);

       //dump_buffer((uint8_t*)desc[desc_idx].addr, desc[desc_idx].len);
    }
}

// dump a vhost vring struct (a vhost vring includes a vring)
void dump_vhost_vring(struct vhost_vring* vring)
{
    fprintf(stdout, "kickfd: 0x%x, callfd: 0x%x\n", vring->kickfd, vring->callfd);
    dump_vring(vring->desc, &vring->avail, &vring->used);
}
