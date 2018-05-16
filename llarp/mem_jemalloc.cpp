#include <jemalloc/jemalloc.h>
#include <llarp/mem.h>

namespace llarp {
  static void *jem_malloc(struct llarp_alloc * mem, size_t sz, size_t align) {
    (void) mem;
    return mallocx(sz, MALLOCX_ALIGN(align));
  }

  static void jem_free(struct llarp_alloc * mem, void *ptr) {
    (void) mem;
    if (ptr) free(ptr);
  }
}  // namespace llarp

extern "C" {
void llarp_mem_jemalloc(struct llarp_alloc * mem) {
  mem->alloc = llarp::jem_malloc;
  mem->free = llarp::jem_free;
}
}
