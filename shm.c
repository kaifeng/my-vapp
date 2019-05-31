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
#include "shm.h"

/*
 * Common code for shared memory
 */

int shm_fds[VHOST_MEMORY_MAX_NREGIONS];

/* 创建一个RW的共享内存 */
void* init_shm(const char* name_prefix, size_t size, int idx)
{
    int fd = 0;
    void* result = 0;
    char path_idx[PATH_MAX];
    int oflags = 0;

    sprintf(path_idx, "%s%d", name_prefix, idx);

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

    result = map_shm_from_fd(fd, size);
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
void* map_shm_from_fd(int fd, size_t size) {
    void *result = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (result == MAP_FAILED) {
        perror("mmap");
        result = 0;
    }
    return result;
}

/* 取消共享内存映射并删除共享内存fd */
int end_shm(const char* name_prefix, void* ptr, size_t size, int idx)
{
    char path_idx[PATH_MAX];

    if (shm_fds[idx] > 0) {
        close(shm_fds[idx]);
    }

    if (munmap(ptr, size) != 0) {
        perror("munmap");
        return -1;
    }

    // server can be null here, something wrong in the code flow.
    // unlink should only performed for who creates the shm.
    if(name_prefix != NULL) {
        sprintf(path_idx, "%s%d", name_prefix, idx);
        LOG("%s: remove shared memory %d, path %s\n", __FUNCTION__, idx, path_idx);
        if (shm_unlink(path_idx) != 0) {
            perror("shm_unlink");
            return -1;
        }
    }

    return 0;
}

/* synchronize a file with a memory map */
int sync_shm(void* ptr, size_t size)
{
    return msync(ptr, size, MS_SYNC | MS_INVALIDATE);
}
