#ifndef LLARP_MEM_HPP
#define LLARP_MEM_HPP
#include <llarp/mem.h>
#include <cmath>
namespace llarp {
template <typename T>
static constexpr size_t alignment() {
  size_t idx = 0;
  size_t sz = sizeof(T);
  while (sz) {
    ++idx;
    sz >>= 1;
  }
  return 1 << idx;
}

template <typename T>
static T *Alloc(llarp_alloc *mem = &llarp_g_mem) {
  return static_cast<T *>(mem->alloc(sizeof(T), alignment<T>()));
}
}  // namespace llarp

#endif
