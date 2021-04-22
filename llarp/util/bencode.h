#pragma once

#include <llarp/constants/proto.hpp>
#include "buffer.hpp"
#include "common.hpp"

#include <functional>

#include <cstdint>

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

/// Write a dictionary entry with a uint64_t value
bool
bencode_write_uint64_entry(llarp_buffer_t* buff, const void* name, size_t sz, uint64_t i);

bool
bencode_start_list(llarp_buffer_t* buff);

bool
bencode_start_dict(llarp_buffer_t* buff);

bool
bencode_end(llarp_buffer_t* buff);

/// read next member, discard it and advance buffer
bool
bencode_discard(llarp_buffer_t* buf);
