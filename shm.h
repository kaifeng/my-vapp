#ifndef SHM_H_
#define SHM_H_

#define SHM_NAME_PREFIX    "/vhost"

// shared memory interface
extern int shm_fds[];
void* create_shm(size_t size, int idx);
void* map_shm(int fd, size_t size);
int unmap_shm(void* ptr, size_t size);
int end_shm(void* ptr, size_t size, int idx);
int sync_shm(void* ptr, size_t size);

#endif
