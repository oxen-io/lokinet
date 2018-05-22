#ifndef LLARP_STRING_H
#define LLARP_STRING_H
#include <llarp/common.h>
#ifdef __cplusplus
extern "C" {
#endif

size_t INLINE
strnlen(const char* str, size_t sz)
{
  size_t slen = 0;
  while(sz-- && str[slen])
    slen++;
  return slen;
}

#ifdef __cplusplus
}
#endif
#endif
