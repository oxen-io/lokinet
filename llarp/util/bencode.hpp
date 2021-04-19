#pragma once

#include "buffer.hpp"
#include "bencode.h"
#include "fs.hpp"
#include <llarp/util/logging/logger.hpp>
#include "mem.hpp"

#include <type_traits>
#include <fstream>
#include <set>
#include <vector>

namespace llarp
{
  template <typename List_t>
  bool
  BEncodeReadList(List_t& result, llarp_buffer_t* buf);

  inline bool
  BEncodeWriteDictMsgType(llarp_buffer_t* buf, const char* k, const char* t)
  {
    return bencode_write_bytestring(buf, k, 1) && bencode_write_bytestring(buf, t, 1);
  }

  template <typename Obj_t>
  bool
  BEncodeWriteDictString(const char* k, const Obj_t& str, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1)
        && bencode_write_bytestring(buf, str.data(), str.size());
  }

  template <typename Obj_t>
  bool
  BEncodeWriteDictEntry(const char* k, const Obj_t& o, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1) && o.BEncode(buf);
  }

  template <typename Int_t>
  bool
  BEncodeWriteDictInt(const char* k, const Int_t& i, llarp_buffer_t* buf)
  {
    if (!bencode_write_bytestring(buf, k, 1))
      return false;
    if constexpr (std::is_enum_v<Int_t>)
      return bencode_write_uint64(buf, static_cast<std::underlying_type_t<Int_t>>(i));
    else
      return bencode_write_uint64(buf, i);
  }

  template <typename List_t>
  bool
  BEncodeMaybeReadDictList(
      const char* k, List_t& item, bool& read, const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key == k)
    {
      if (!BEncodeReadList(item, buf))
      {
        return false;
      }
      read = true;
    }
    return true;
  }

  template <typename Item_t>
  bool
  BEncodeMaybeReadDictEntry(
      const char* k, Item_t& item, bool& read, const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key == k)
    {
      if (!item.BDecode(buf))
      {
        llarp::LogWarnTag("llarp/bencode.hpp", "failed to decode key ", k, " for entry in dict");

        return false;
      }
      read = true;
    }
    return true;
  }

  template <typename Int_t>
  bool
  BEncodeMaybeReadDictInt(
      const char* k, Int_t& i, bool& read, const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key == k)
    {
      uint64_t read_i;
      if (!bencode_read_integer(buf, &read_i))
      {
        llarp::LogWarnTag("llarp/BEncode.hpp", "failed to decode key ", k, " for integer in dict");
        return false;
      }

      i = static_cast<Int_t>(read_i);
      read = true;
    }
    return true;
  }

  /// If the key matches, reads in the version and ensures that it equals the
  /// expected version
  template <typename Item_t>
  bool
  BEncodeMaybeVerifyVersion(
      const char* k,
      Item_t& item,
      uint64_t expect,
      bool& read,
      const llarp_buffer_t& key,
      llarp_buffer_t* buf)
  {
    if (key == k)
    {
      if (!bencode_read_integer(buf, &item))
        return false;
      read = item == expect;
    }
    return true;
  }

  template <typename List_t>
  bool
  BEncodeWriteDictBEncodeList(const char* k, const List_t& l, llarp_buffer_t* buf)
  {
    if (!bencode_write_bytestring(buf, k, 1))
      return false;
    if (!bencode_start_list(buf))
      return false;

    for (const auto& item : l)
      if (!item->BEncode(buf))
        return false;
    return bencode_end(buf);
  }

  template <typename Array>
  bool
  BEncodeWriteDictArray(const char* k, const Array& array, llarp_buffer_t* buf)
  {
    if (!bencode_write_bytestring(buf, k, 1))
      return false;
    if (!bencode_start_list(buf))
      return false;

    for (size_t idx = 0; idx < array.size(); ++idx)
      if (!array[idx].BEncode(buf))
        return false;
    return bencode_end(buf);
  }

  template <typename Iter>
  bool
  BEncodeWriteList(Iter itr, Iter end, llarp_buffer_t* buf)
  {
    if (!bencode_start_list(buf))
      return false;
    while (itr != end)
      if (!itr->BEncode(buf))
        return false;
      else
        ++itr;
    return bencode_end(buf);
  }

  template <typename Sink>
  bool
  bencode_read_dict(Sink&& sink, llarp_buffer_t* buffer)
  {
    if (buffer->size_left() < 2)  // minimum case is 'de'
      return false;
    if (*buffer->cur != 'd')  // ensure is a dictionary
      return false;
    buffer->cur++;
    while (buffer->size_left() && *buffer->cur != 'e')
    {
      llarp_buffer_t strbuf;  // temporary buffer for current element
      if (bencode_read_string(buffer, &strbuf))
      {
        if (!sink(buffer, &strbuf))  // check for early abort
          return false;
      }
      else
        return false;
    }

    if (*buffer->cur != 'e')
    {
      llarp::LogWarn("reading dict not ending on 'e'");
      // make sure we're at dictionary end
      return false;
    }
    buffer->cur++;
    return sink(buffer, nullptr);
  }

  template <typename Sink>
  bool
  bencode_decode_dict(Sink&& sink, llarp_buffer_t* buff)
  {
    return bencode_read_dict(
        [&](llarp_buffer_t* buffer, llarp_buffer_t* key) {
          if (key == nullptr)
            return true;
          if (sink.DecodeKey(*key, buffer))
            return true;
          llarp::LogWarnTag(
              "llarp/bencode.hpp", "undefined key '", *key->cur, "' for entry in dict");

          return false;
        },
        buff);
  }

  template <typename Sink>
  bool
  bencode_read_list(Sink&& sink, llarp_buffer_t* buffer)
  {
    if (buffer->size_left() < 2)  // minimum case is 'le'
      return false;
    if (*buffer->cur != 'l')  // ensure is a list
    {
      llarp::LogWarn("bencode::bencode_read_list - expecting list got ", *buffer->cur);
      return false;
    }

    buffer->cur++;
    while (buffer->size_left() && *buffer->cur != 'e')
    {
      if (!sink(buffer, true))  // check for early abort
        return false;
    }
    if (*buffer->cur != 'e')  // make sure we're at a list end
      return false;
    buffer->cur++;
    return sink(buffer, false);
  }

  template <typename Array>
  bool
  BEncodeReadArray(Array& array, llarp_buffer_t* buf)
  {
    size_t idx = 0;
    return bencode_read_list(
        [&array, &idx](llarp_buffer_t* buffer, bool has) {
          if (has)
          {
            if (idx >= array.size())
              return false;
            if (!array[idx++].BDecode(buffer))
              return false;
          }
          return true;
        },
        buf);
  }
  template <typename List_t>
  bool
  BEncodeReadList(List_t& result, llarp_buffer_t* buf)
  {
    return bencode_read_list(
        [&result](llarp_buffer_t* buffer, bool has) {
          if (has)
          {
            if (!result.emplace(result.end())->BDecode(buffer))
            {
              return false;
            }
          }
          return true;
        },
        buf);
  }

  /// read a std::set of decodable entities and deny duplicates
  template <typename Set_t>
  bool
  BEncodeReadSet(Set_t& set, llarp_buffer_t* buffer)
  {
    return bencode_read_list(
        [&set](llarp_buffer_t* buf, bool more) {
          if (more)
          {
            typename Set_t::value_type item;
            if (not item.BDecode(buf))
              return false;
            // deny duplicates
            return set.emplace(item).second;
          }
          return true;
        },
        buffer);
  }

  /// write an iterable container as a list
  template <typename Set_t>
  bool
  BEncodeWriteSet(const Set_t& set, llarp_buffer_t* buffer)
  {
    if (not bencode_start_list(buffer))
      return false;

    for (const auto& item : set)
    {
      if (not item.BEncode(buffer))
        return false;
    }

    return bencode_end(buffer);
  }

  template <typename List_t>
  bool
  BEncodeWriteDictList(const char* k, List_t& list, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1) && BEncodeWriteList(list.begin(), list.end(), buf);
  }

  template <size_t bufsz, typename T>
  void
  Dump(const T& val)
  {
    std::array<byte_t, bufsz> tmp;
    llarp_buffer_t buf(tmp);
    if (val.BEncode(&buf))
    {
      llarp::DumpBuffer<decltype(buf), 128>(buf);
    }
  }

  /// read entire file and decode its contents into t
  template <typename T>
  bool
  BDecodeReadFile(const fs::path fpath, T& t)
  {
    std::vector<byte_t> ptr;
    {
      std::ifstream f;
      f.open(fpath.string());
      if (!f.is_open())
      {
        return false;
      }
      f.seekg(0, std::ios::end);
      const std::streampos sz = f.tellg();
      f.seekg(0, std::ios::beg);
      ptr.resize(sz);
      f.read((char*)ptr.data(), sz);
    }
    llarp_buffer_t buf(ptr);
    return t.BDecode(&buf);
  }

  /// bencode and write to file
  template <typename T, size_t bufsz>
  bool
  BEncodeWriteFile(const fs::path fpath, const T& t)
  {
    std::array<byte_t, bufsz> tmp;
    llarp_buffer_t buf(tmp);
    if (!t.BEncode(&buf))
      return false;
    buf.sz = buf.cur - buf.base;
    {
      auto f = llarp::util::OpenFileStream<std::ofstream>(fpath, std::ios::binary);
      if (not f or not f->is_open())
        return false;
      f->write((char*)buf.base, buf.sz);
    }
    return true;
  }

  /// seek for an int in a dict with a key k used for version
  /// set v to parsed value if found
  /// this call rewinds the buffer unconditionally before return
  /// returns false only if there was
  template <typename Int_t>
  bool
  BEncodeSeekDictVersion(Int_t& v, llarp_buffer_t* buf, const byte_t k)
  {
    const auto ret = bencode_read_dict(
        [&v, k](llarp_buffer_t* buffer, llarp_buffer_t* key) -> bool {
          if (key == nullptr)
            return true;
          if (key->sz == 1 && *key->cur == k)
          {
            return bencode_read_integer(buffer, &v);
          }
          return bencode_discard(buffer);
        },
        buf);
    // rewind
    buf->cur = buf->base;
    return ret;
  }

}  // namespace llarp
