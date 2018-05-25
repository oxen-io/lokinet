#ifndef LLARP_MEM_HPP
#define LLARP_MEM_HPP
#include <llarp/buffer.h>
#include <llarp/mem.h>
#include <stdio.h>

namespace llarp
{
  template < typename T >
  static constexpr size_t
  alignment()
  {
    size_t idx = 0;
    size_t sz  = sizeof(T);
    while(sz)
    {
      ++idx;
      sz >>= 1;
    }
    return 1 << idx;
  }

  template < typename T >
  static T *
  Alloc(llarp_alloc *mem)
  {
    return static_cast< T * >(mem->alloc(mem, sizeof(T), alignment< T >()));
  }

  void
  Zero(void *ptr, size_t sz);

  template < typename T >
  void
  dumphex(const uint8_t *t)
  {
    size_t idx = 0;
    while(idx < sizeof(T))
    {
      printf("%.2x ", t[idx++]);
      if(idx % 8 == 0)
        printf("\n");
    }
  }

  template < typename T >
  void
  dumphex_buffer(T buff)
  {
    size_t idx = 0;
    printf("buffer of size %ld\n", buff.sz);
    while(idx < buff.sz)
    {
      printf("%.2x ", buff.base[idx++]);
      if(idx % 8 == 0)
        printf("\n");
    }
  }

}  // namespace llarp

#endif
