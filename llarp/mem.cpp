#include <llarp/mem.h>

extern "C" {
  struct llarp_alloc llarp_g_mem = {
    .malloc = nullptr,
    .realloc = nullptr,
    .calloc = nullptr,
    .free = nullptr
  };
}
