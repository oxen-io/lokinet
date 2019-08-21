#define NO_JEMALLOC
#include <util/mem.h>
#include <cstdlib>

#include <absl/base/attributes.h>

namespace llarp
{
  void
  Zero(void *ptr, size_t sz)
  {
    auto *p = (uint8_t *)ptr;
    while(sz--)
    {
      *p = 0;
      ++p;
    }
  }
}  // namespace llarp

void
llarp_mem_slab(ABSL_ATTRIBUTE_UNUSED struct llarp_alloc *mem,
               ABSL_ATTRIBUTE_UNUSED uint32_t *buf,
               ABSL_ATTRIBUTE_UNUSED size_t sz)
{
  // not implemented
  abort();
}

bool
llarp_eq(const void *a, const void *b, size_t sz)
{
  bool result       = true;
  const auto *a_ptr = (const uint8_t *)a;
  const auto *b_ptr = (const uint8_t *)b;
  while(sz--)
  {
    result &= a_ptr[sz] == b_ptr[sz];
  }
  return result;
}
