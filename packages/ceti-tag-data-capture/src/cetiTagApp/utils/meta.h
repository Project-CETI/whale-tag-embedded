#ifndef __LIB_WHALE_TAG_META_H__
#define __LIB_WHALE_TAG_META_H__

#include "../_versioning.h"

#include <stdint.h>

int meta_log(uint64_t timestamp);
void *meta_log_thread(void *paramPtr);

#endif