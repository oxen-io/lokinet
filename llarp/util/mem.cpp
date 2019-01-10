#define NO_JEMALLOC
#include <util/mem.h>
#include <cstdlib>

namespace llarp
{
  void
  Zero(void *ptr, size_t sz)
  {
    uint8_t *p = (uint8_t *)ptr;
    while(sz--)
    {
      *p = 0;
      ++p;
    }
  }
}  // namespace llarp

void
llarp_mem_slab(__attribute__((unused)) struct llarp_alloc *mem,
               __attribute__((unused)) uint32_t *buf,
               __attribute__((unused)) size_t sz)
{
  // not implemented
  abort();
}

bool
llarp_eq(const void *a, const void *b, size_t sz)
{
  bool result          = true;
  const uint8_t *a_ptr = (const uint8_t *)a;
  const uint8_t *b_ptr = (const uint8_t *)b;
  while(sz--)
  {
    result &= a_ptr[sz] == b_ptr[sz];
  }
  return result;
}
