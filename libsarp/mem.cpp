#include <sarp/mem.h>

extern "C" {
  struct sarp_alloc sarp_g_mem = {
    .malloc = nullptr,
    .realloc = nullptr,
    .calloc = nullptr,
    .free = nullptr
  };
}
