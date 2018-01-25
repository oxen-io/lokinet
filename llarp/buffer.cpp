#include <sarp/buffer.h>

extern "C" {

  size_t sarp_buffer_size_left(sarp_buffer_t * buff)
  {
    std::ptrdiff_t diff = buff->cur - buff->base;
    if(diff < 0)
    {
      return 0;
    }
    else if(diff > buff->sz) return 0;
    else return buff->sz - diff;
  }

}
