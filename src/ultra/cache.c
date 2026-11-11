// libultra's write back of the data cache, on libdragon's calls.
#include <n64sys.h>

#include "ultra.h"

// libultra's osWritebackDCache: writes back the lines of the data cache that
// hold a block. A block of the size of the cache or more is all of the cache,
// and each line of it is then written back and dropped.
void osWritebackDCache(void *address, s32 size)
{
    if (size <= 0) {
        return;
    }
    if ((u32)size < 0x2000) {
        data_cache_hit_writeback(address, size);
    } else {
        data_cache_writeback_invalidate_all();
    }
}

// libultra's osWritebackDCacheAll: writes back and drops each line of the
// data cache.
void osWritebackDCacheAll(void)
{
    data_cache_writeback_invalidate_all();
}
