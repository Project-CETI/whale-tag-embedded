//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------
#include "memory.h"

#include "logging.h"

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

void *create_shared_memory_region(const char *name, size_t size) {
    void *address = NULL;
    //=== setup battery shared memory ===
  // open/create ipc file
  int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0644);
  if (shm_fd < 0) {
    perror("shm_open");
    CETI_ERR("Failed to open/create shared memory");
    return NULL;
  }

  // size to sample size
  if (ftruncate(shm_fd, size)){
    perror("shm_fd");
    CETI_ERR("Failed to resize shared memory");
    return NULL;
  }

  // memory map address
  address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if(address == MAP_FAILED){
    perror("mmap");
    CETI_ERR("Failed to map shared memory");
    return NULL;
  }

  close(shm_fd);
  return address;
}
