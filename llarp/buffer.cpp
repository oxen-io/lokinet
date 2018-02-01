#include <llarp/buffer.h>
#include <cstring>

extern "C" {

size_t llarp_buffer_size_left(llarp_buffer_t *buff) {
  auto diff = buff->cur - buff->base;
  if (diff < 0) {
    return 0;
  } else if (diff > buff->sz)
    return 0;
  else
    return buff->sz - diff;
}

bool llarp_buffer_write(llarp_buffer_t *buff, const void *data, size_t sz) {
  size_t left = llarp_buffer_size_left(buff);
  if (sz > left) {
    return false;
  }
  std::memcpy(buff->cur, data, sz);
  buff->cur += sz;
  return true;
}
}
