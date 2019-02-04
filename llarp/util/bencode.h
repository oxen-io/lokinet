#ifndef LLARP_BENCODE_H
#define LLARP_BENCODE_H

#include <constants/proto.hpp>
#include <util/buffer.hpp>
#include <util/common.hpp>

#include <functional>
#include <stdbool.h>
#include <stdint.h>

/**
 * bencode.h
 *
 * helper functions for handling bencoding
 * https://en.wikipedia.org/wiki/Bencode for more information on the format
 * we utilize llarp_buffer which provides memory management
 */

bool
bencode_read_integer(llarp_buffer_t* buffer, uint64_t* result);

bool
bencode_read_string(llarp_buffer_t* buffer, llarp_buffer_t* result);

bool
bencode_write_bytestring(llarp_buffer_t* buff, const void* data, size_t sz);

bool
bencode_write_uint64(llarp_buffer_t* buff, uint64_t i);

bool
bencode_write_version_entry(llarp_buffer_t* buff);

bool
bencode_start_list(llarp_buffer_t* buff);

bool
bencode_start_dict(llarp_buffer_t* buff);

bool
bencode_end(llarp_buffer_t* buff);

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
  std::function< bool(dict_reader*, llarp_buffer_t*) > on_key;
};

bool
bencode_read_dict(llarp_buffer_t* buff, struct dict_reader* r);

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
  std::function< bool(list_reader*, bool) > on_item;
};

bool
bencode_read_list(llarp_buffer_t* buff, struct list_reader* r);

#endif
