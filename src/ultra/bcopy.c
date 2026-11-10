#include <string.h>

#include "ultra.h"

// libultra's bcopy: the blocks may overlap.
void ultra_bcopy(const void *src, void *dst, u32 len)
{
    memmove(dst, src, len);
}
