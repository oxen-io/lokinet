#ifndef LLARP_BENCODE_H
#define LLARP_BENCODE_H
#include <llarp/common.h>
#include <stdbool.h>
#include <stdint.h>
#include <llarp/buffer.h>
#include <llarp/version.h>

#ifdef __cplusplus
extern "C" {
#endif

  bool INLINE bencode_write_bytestring(llarp_buffer_t * buff, const void * data, size_t sz)
  {
    if(!llarp_buffer_writef(buff, "%ld:", sz)) return false;
    return llarp_buffer_write(buff, data, sz);
  }

  bool INLINE bencode_write_int(llarp_buffer_t * buff, int i)
  {
    return llarp_buffer_writef(buff, "i%de", i);
  }
  
  bool INLINE bencode_write_uint16(llarp_buffer_t * buff, uint16_t i)
  {
    return llarp_buffer_writef(buff, "i%de", i);
  }

  bool INLINE bencode_write_int64(llarp_buffer_t * buff, int64_t i)
  {
    return llarp_buffer_writef(buff, "i%lde", i);
  }
  
  bool INLINE bencode_write_uint64(llarp_buffer_t * buff, uint64_t i)
  {
    return llarp_buffer_writef(buff, "i%lde", i);
  }

  bool INLINE bencode_write_sizeint(llarp_buffer_t * buff, size_t i)
  {
    return llarp_buffer_writef(buff, "i%lde", i);
  }

  bool INLINE bencode_start_list(llarp_buffer_t * buff)
  {
    return llarp_buffer_write(buff, "l", 1);
  }
  
  bool INLINE bencode_start_dict(llarp_buffer_t * buff)
  {
    return llarp_buffer_write(buff, "d", 1);
  }
  
  bool INLINE bencode_end(llarp_buffer_t * buff)
  {
    return llarp_buffer_write(buff, "e", 1);
  }

  bool INLINE bencode_write_version_entry(llarp_buffer_t * buff)
  {
    return llarp_buffer_writef(buff, "1:vi%de", LLARP_PROTO_VERSION);
  }
  
#ifdef __cplusplus
}
#endif
#endif
