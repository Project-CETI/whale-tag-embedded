//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------

#ifndef CETI_MEMORY_H
#define CETI_MEMORY_H

#include <unistd.h>

void *create_shared_memory_region(const char *name, size_t size);

#endif // CETI_MEMORY_H
