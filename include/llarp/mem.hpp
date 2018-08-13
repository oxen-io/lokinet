#ifndef LLARP_MEM_HPP
#define LLARP_MEM_HPP
#include <llarp/buffer.h>
#include <llarp/mem.h>
#include <cctype>
#include <cstdio>

namespace llarp
{
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

  template < typename T, size_t align = 128 >
  void
  DumpBuffer(const T &buff)
  {
    size_t idx = 0;
    printf("buffer of size %zu\n", buff.sz);
    while(idx < buff.sz)
    {
      if(buff.base + idx == buff.cur)
      {
        printf("%c[1;31m", 27);
      }
      if(std::isprint(buff.base[idx]))
      {
        printf("%c", buff.base[idx]);
      }
      else
      {
        printf("X");
      }
      if(buff.base + idx == buff.cur)
      {
        printf("%c[0;0m", 27);
      }
      ++idx;
      if(idx % align == 0)
        printf("\n");
    }
    printf("\n");
    fflush(stdout);
  }

}  // namespace llarp

#endif
