#ifndef LLARP_MEM_H_
#define LLARP_MEM_H_

#include <cstdint>
#include <cstdlib>

/** constant time memcmp */
bool
llarp_eq(const void* a, const void* b, size_t sz);

#endif
