#include <llarp/mem.h>

extern "C" {
  struct llarp_alloc llarp_g_mem = {
    .alloc = nullptr,
    .free = nullptr
  };
}
