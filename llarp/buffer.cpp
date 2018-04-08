#include <llarp/buffer.h>
#include <stdarg.h>
#include <stdio.h>

extern "C" {
bool llarp_buffer_writef(llarp_buffer_t* buff, const char* fmt, ...) {
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

  bool llarp_buffer_readfile(llarp_buffer_t * buff, FILE * f, llarp_alloc * mem)  
  {
    ssize_t len;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    rewind(f);
    if(len > 0)
    {
      buff->base = static_cast<char *>(mem->alloc(len, 8));
      buff->cur = buff->base;
      buff->sz = len;
      size_t sz = fread(buff->base, len, 1, f);
      rewind(f);
      return sz == len;
    }
    return false;
  }
}
