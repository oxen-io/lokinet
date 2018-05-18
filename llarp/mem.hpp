#ifndef LLARP_MEM_HPP
#define LLARP_MEM_HPP
#include <llarp/mem.h>
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
static T *Alloc(llarp_alloc *mem) {
  return static_cast<T *>(mem->alloc(mem, sizeof(T), alignment<T>()));
}

  void Zero(void * ptr, size_t sz);
  
}  // namespace llarp


#endif
