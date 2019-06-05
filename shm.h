#ifndef SHM_H_
#define SHM_H_

// shared memory interface
extern int shm_fds[];
void* create_shm(const char* path, size_t size, int idx);
void* map_shm(int fd, size_t size);
int end_shm(const char* path, void* ptr, size_t size, int idx);
int sync_shm(void* ptr, size_t size);

#endif
