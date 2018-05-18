#include <llarp/mem.h>

namespace llarp
{
  void Zero(void * ptr, size_t sz)
  {
    uint8_t * p = (uint8_t *) ptr;
    while(sz--)
    {
      *p = 0;
      ++p;
    }
  }
}

extern "C" {

  void llarp_mem_slab(struct llarp_alloc * mem, uint32_t * buf, size_t sz)
  {
    // not implemented
    abort();
  }
}
