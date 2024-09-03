#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

void *shm_open_read(const char *pName, size_t size) {
    int shm_fd = shm_open(pName, O_RDWR, 0444);
    if (shm_fd < 0) {
        perror("shm_open");
        return NULL;
    }
    // size to sample size
    if (ftruncate(shm_fd, size)){
        perror("ftruncate");
        close(shm_fd);
        return NULL;
    }
    // memory map address
    void *shm_ptr = mmap(NULL, size, PROT_READ , MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    if(shm_ptr == MAP_FAILED){
        perror("mmap");
        return NULL;
    }
    return shm_ptr;
}