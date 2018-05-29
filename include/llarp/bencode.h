#ifndef LLARP_BENCODE_H
#define LLARP_BENCODE_H
#include <llarp/buffer.h>
#include <llarp/common.h>
#include <llarp/proto.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * bencode.h
 *
 * helper functions for handling bencoding
 * https://en.wikipedia.org/wiki/Bencode for more information on the format
 * we utilize llarp_buffer which provides memory management
 */

#ifdef __cplusplus
extern "C" {
#endif

static bool INLINE
bencode_write_bytestring(llarp_buffer_t* buff, const void* data, size_t sz)
{
  if(!llarp_buffer_writef(buff, "%ld:", sz))
    return false;
  return llarp_buffer_write(buff, data, sz);
}

static bool INLINE
bencode_write_int(llarp_buffer_t* buff, int i)
{
  return llarp_buffer_writef(buff, "i%de", i);
}

static bool INLINE
bencode_write_uint16(llarp_buffer_t* buff, uint16_t i)
{
  return llarp_buffer_writef(buff, "i%de", i);
}

static bool INLINE
bencode_write_int64(llarp_buffer_t* buff, int64_t i)
{
  return llarp_buffer_writef(buff, "i%lde", i);
}

static bool INLINE
bencode_write_uint64(llarp_buffer_t* buff, uint64_t i)
{
  return llarp_buffer_writef(buff, "i%lde", i);
}

static bool INLINE
bencode_write_sizeint(llarp_buffer_t* buff, size_t i)
{
  return llarp_buffer_writef(buff, "i%lde", i);
}

static bool INLINE
bencode_start_list(llarp_buffer_t* buff)
{
  return llarp_buffer_write(buff, "l", 1);
}

static bool INLINE
bencode_start_dict(llarp_buffer_t* buff)
{
  return llarp_buffer_write(buff, "d", 1);
}

static bool INLINE
bencode_end(llarp_buffer_t* buff)
{
  return llarp_buffer_write(buff, "e", 1);
}

static bool INLINE
bencode_write_version_entry(llarp_buffer_t* buff)
{
  return llarp_buffer_writef(buff, "1:vi%de", LLARP_PROTO_VERSION);
}

static bool INLINE
bdecode_read_integer(struct llarp_buffer_t* buffer, uint64_t* result)
{
  size_t len;
  if(*buffer->cur != 'i')
    return false;

  char numbuf[32];

  buffer->cur++;

  len =
      llarp_buffer_read_until(buffer, 'e', (byte_t*)numbuf, sizeof(numbuf) - 1);
  if(!len)
    return false;

  buffer->cur++;

  numbuf[len] = 0;
  *result     = atol(numbuf);
  return true;
}

static bool INLINE
bdecode_read_string(llarp_buffer_t* buffer, llarp_buffer_t* result)
{
  size_t len, slen;
  int num;
  char numbuf[10];

  len =
      llarp_buffer_read_until(buffer, ':', (byte_t*)numbuf, sizeof(numbuf) - 1);
  if(!len)
    return false;

  numbuf[len] = 0;
  num         = atoi(numbuf);
  if(num < 0)
    return false;

  slen = num;

  buffer->cur++;

  len = llarp_buffer_size_left(*buffer);
  if(len < slen)
    return false;

  result->base = buffer->cur;
  result->cur  = buffer->cur;
  result->sz   = slen;
  buffer->cur += slen;
  return true;
}

struct dict_reader
{
  /// makes passing data into on_key easier
  llarp_buffer_t* buffer;
  /// not currently used, maybe used in the future to pass additional
  /// information to on_key
  void* user;
  /**
   * called when we got a key string, return true to continue iteration
   * called with null key on done iterating
   */
  bool (*on_key)(struct dict_reader*, llarp_buffer_t*);
};

static bool INLINE
bdecode_read_dict(llarp_buffer_t* buff, struct dict_reader* r)
{
  llarp_buffer_t strbuf;      // temporary buffer for current element
  r->buffer = buff;           // set up dict_reader
  if(*r->buffer->cur != 'd')  // ensure is a dictionary
    return false;
  r->buffer->cur++;
  while(llarp_buffer_size_left(*r->buffer) && *r->buffer->cur != 'e')
  {
    if(bdecode_read_string(r->buffer, &strbuf))
    {
      if(!r->on_key(r, &strbuf))  // check for early abort
        return false;
    }
  }

  if(*r->buffer->cur != 'e')  // make sure we're at dictionary end
    return false;
  r->buffer->cur++;
  return r->on_key(r, 0);
}

struct list_reader
{
  /// makes passing data into on_item easier
  llarp_buffer_t* buffer;
  /// not currently used, maybe used in the future to pass additional
  /// information to on_item
  void* user;
  /**
   * called with true when we got an element, return true to continue iteration
   * called with false on iteration completion
   */
  bool (*on_item)(struct list_reader*, bool);
};

static bool INLINE
bdecode_read_list(llarp_buffer_t* buff, struct list_reader* r)
{
  r->buffer = buff;
  if(*r->buffer->cur != 'l')  // ensure is a list
    return false;

  r->buffer->cur++;
  while(llarp_buffer_size_left(*r->buffer) && *r->buffer->cur != 'e')
  {
    if(!r->on_item(r, true))  // check for early abort
      return false;
  }
  if(*r->buffer->cur != 'e')  // make sure we're at a list end
    return false;
  r->buffer->cur++;
  return r->on_item(r, false);
}

#ifdef __cplusplus
}
#endif
#endif
