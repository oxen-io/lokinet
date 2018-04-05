#include <llarp/mem.h>
#include <cstring>

namespace llarp {
void *std_malloc(size_t sz, size_t align) {
  (void)align;
  void *ptr = malloc(sz);
  if (ptr) {
    std::memset(ptr, 0, sz);
    return ptr;
  }
  abort();
}

void std_free(void *ptr) {
  if (ptr) free(ptr);
}

}  // namespace llarp

extern "C" {
void llarp_mem_stdlib() {
  llarp_g_mem.alloc = llarp::std_malloc;
  llarp_g_mem.free = llarp::std_free;
}
}
