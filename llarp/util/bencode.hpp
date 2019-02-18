#ifndef LLARP_BENCODE_HPP
#define LLARP_BENCODE_HPP

#include <util/buffer.hpp>
#include <util/bencode.h>
#include <util/logger.hpp>
#include <util/mem.hpp>

#include <fstream>
#include <set>
#include <vector>

namespace llarp
{
  inline bool
  BEncodeWriteDictMsgType(llarp_buffer_t* buf, const char* k, const char* t)
  {
    return bencode_write_bytestring(buf, k, 1)
        && bencode_write_bytestring(buf, t, 1);
  }

  template < typename Obj_t >
  bool
  BEncodeWriteDictString(const char* k, const Obj_t& str, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1)
        && bencode_write_bytestring(buf, str.data(), str.size());
  }

  template < typename Obj_t >
  bool
  BEncodeWriteDictEntry(const char* k, const Obj_t& o, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1) && o.BEncode(buf);
  }

  template < typename Int_t >
  bool
  BEncodeWriteDictInt(const char* k, const Int_t& i, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1) && bencode_write_uint64(buf, i);
  }

  template < typename List_t >
  bool
  BEncodeMaybeReadDictList(const char* k, List_t& item, bool& read,
                           const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if(key == k)
    {
      if(!BEncodeReadList(item, buf))
      {
        return false;
      }
      read = true;
    }
    return true;
  }

  template < typename Item_t >
  bool
  BEncodeMaybeReadDictEntry(const char* k, Item_t& item, bool& read,
                            const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if(key == k)
    {
      if(!item.BDecode(buf))
      {
        llarp::LogWarnTag("llarp/bencode.hpp", "failed to decode key ", k,
                          " for entry in dict");

        return false;
      }
      read = true;
    }
    return true;
  }

  template < typename Int_t >
  bool
  BEncodeMaybeReadDictInt(const char* k, Int_t& i, bool& read,
                          const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if(key == k)
    {
      if(!bencode_read_integer(buf, &i))
      {
        llarp::LogWarnTag("llarp/BEncode.hpp", "failed to decode key ", k,
                          " for integer in dict");
        return false;
      }
      read = true;
    }
    return true;
  }

  template < typename Item_t >
  bool
  BEncodeMaybeReadVersion(const char* k, Item_t& item, uint64_t expect,
                          bool& read, const llarp_buffer_t& key,
                          llarp_buffer_t* buf)
  {
    if(key == k)
    {
      if(!bencode_read_integer(buf, &item))
        return false;
      read = item == expect;
    }
    return true;
  }

  template < typename List_t >
  bool
  BEncodeWriteDictBEncodeList(const char* k, const List_t& l,
                              llarp_buffer_t* buf)
  {
    if(!bencode_write_bytestring(buf, k, 1))
      return false;
    if(!bencode_start_list(buf))
      return false;

    for(const auto& item : l)
      if(!item->BEncode(buf))
        return false;
    return bencode_end(buf);
  }

  template < typename Array >
  bool
  BEncodeWriteDictArray(const char* k, const Array& array, llarp_buffer_t* buf)
  {
    if(!bencode_write_bytestring(buf, k, 1))
      return false;
    if(!bencode_start_list(buf))
      return false;

    for(size_t idx = 0; idx < array.size(); ++idx)
      if(!array[idx].BEncode(buf))
        return false;
    return bencode_end(buf);
  }

  template < typename Array >
  bool
  BEncodeReadArray(Array& array, llarp_buffer_t* buf)
  {
    if(*buf->cur != 'l')  // ensure is a list
      return false;

    buf->cur++;
    size_t idx = 0;
    while(buf->size_left() && *buf->cur != 'e')
    {
      if(idx >= array.size())
        return false;
      if(!array[idx++].BDecode(buf))
        return false;
    }
    if(*buf->cur != 'e')  // make sure we're at a list end
      return false;
    buf->cur++;
    return true;
  }

  template < typename Iter >
  bool
  BEncodeWriteList(Iter itr, Iter end, llarp_buffer_t* buf)
  {
    if(!bencode_start_list(buf))
      return false;
    while(itr != end)
      if(!itr->BEncode(buf))
        return false;
      else
        ++itr;
    return bencode_end(buf);
  }

  template < typename List_t >
  bool
  BEncodeReadList(List_t& result, llarp_buffer_t* buf)
  {
    if(*buf->cur != 'l')  // ensure is a list
      return false;

    buf->cur++;
    while(buf->size_left() && *buf->cur != 'e')
    {
      if(!result.emplace(result.end())->BDecode(buf))
        return false;
    }
    if(*buf->cur != 'e')  // make sure we're at a list end
      return false;
    buf->cur++;
    return true;
  }

  template < typename T >
  bool
  BEncodeReadSet(std::set< T >& result, llarp_buffer_t* buf)
  {
    if(*buf->cur != 'l')  // ensure is a list
      return false;

    buf->cur++;
    while(buf->size_left() && *buf->cur != 'e')
    {
      T item;
      if(!item.BDecode(buf))
        return false;
      return result.insert(item).second;
    }
    if(*buf->cur != 'e')  // make sure we're at a list end
      return false;
    buf->cur++;
    return true;
  }

  template < typename List_t >
  bool
  BEncodeWriteDictList(const char* k, List_t& list, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1)
        && BEncodeWriteList(list.begin(), list.end(), buf);
  }

  /// bencode serializable message
  struct IBEncodeMessage
  {
    virtual ~IBEncodeMessage(){};

    IBEncodeMessage(uint64_t v = LLARP_PROTO_VERSION)
    {
      version = v;
    }

    virtual bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) = 0;

    virtual bool
    BEncode(llarp_buffer_t* buf) const = 0;

    virtual bool
    BDecode(llarp_buffer_t* buf)
    {
      dict_reader r;
      r.user   = this;
      r.on_key = &OnKey;
      return bencode_read_dict(buf, &r);
    }

    // TODO: check for shadowed values elsewhere
    uint64_t version = 0;

    static bool
    OnKey(dict_reader* r, llarp_buffer_t* k)
    {
      return static_cast< IBEncodeMessage* >(r->user)->HandleKey(k, r->buffer);
    }

    bool
    HandleKey(llarp_buffer_t* k, llarp_buffer_t* val)
    {
      if(k == nullptr)
        return true;
      if(DecodeKey(*k, val))
        return true;
      llarp::LogWarnTag("llarp/bencode.hpp", "undefined key '", *k->cur,
                        "' for entry in dict");

      return false;
    }

    template < size_t bufsz, size_t align = 128 >
    void
    Dump() const
    {
      std::array< byte_t, bufsz > tmp;
      llarp_buffer_t buf(tmp);
      if(BEncode(&buf))
      {
        llarp::DumpBuffer< decltype(buf), align >(buf);
      }
    }
  };

  /// read entire file and decode its contents into t
  template < typename T >
  bool
  BDecodeReadFile(const char* fpath, T& t)
  {
    std::vector< byte_t > ptr;
    {
      std::ifstream f;
      f.open(fpath);
      if(!f.is_open())
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
    auto result = t.BDecode(&buf);
    if(!result)
    {
      DumpBuffer(buf);
    }
    return result;
  }

  /// bencode and write to file
  template < typename T, size_t bufsz >
  bool
  BEncodeWriteFile(const char* fpath, const T& t)
  {
    std::array< byte_t, bufsz > tmp;
    llarp_buffer_t buf(tmp);
    if(!t.BEncode(&buf))
      return false;
    buf.sz = buf.cur - buf.base;
    {
      std::ofstream f;
      f.open(fpath);
      if(!f.is_open())
        return false;
      f.write((char*)buf.base, buf.sz);
    }
    return true;
  }

}  // namespace llarp

#endif
