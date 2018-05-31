#define NO_JEMALLOC
#include <llarp/mem.h>
#include <cstdlib>
#include <cstring>

struct llarp_alloc {
    void *(*alloc)(struct llarp_alloc *mem, size_t sz, size_t align);
    void (*free)(struct llarp_alloc *mem, void *ptr);
};

namespace llarp
{
  void *
  std_malloc(struct llarp_alloc *mem, size_t sz, size_t align)
  {
    (void)mem;
    (void)align;
    void *ptr = malloc(sz);
    if(ptr)
    {
      std::memset(ptr, 0, sz);
      return ptr;
    }
    abort();
  }

  void
  std_free(struct llarp_alloc *mem, void *ptr)
  {
    (void)mem;
    if(ptr)
      free(ptr);
  }

}  // namespace llarp

extern "C" {
void
llarp_mem_stdlib(struct llarp_alloc *mem)
{
  mem->alloc = llarp::std_malloc;
  mem->free  = llarp::std_free;
}
}
