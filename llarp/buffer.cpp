#include <llarp/buffer.h>
#include <stdarg.h>
#include <stdio.h>

extern "C" {
  bool llarp_buffer_writef(llarp_buffer_t * buff, const char * fmt, ...)
  {
    int written;
    size_t sz = llarp_buffer_size_left(buff);
    va_list args;
    va_start(args, fmt);
    written = snprintf(buff->cur, sz, fmt, args);
    va_end(args);
    if (written == -1) return false;
    buff->sz += written;
    return true;
  }
}
