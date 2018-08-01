#ifndef LLARP_BENCODE_HPP
#define LLARP_BENCODE_HPP

#include <llarp/bencode.h>
#include <llarp/logger.hpp>

#include <set>

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
        && bencode_write_bytestring(buf, str.c_str(), str.size());
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

  template < typename Item_t >
  bool
  BEncodeMaybeReadDictEntry(const char* k, Item_t& item, bool& read,
                            llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, k))
    {
      if(!item.BDecode(buf))
      {
        llarp::LogWarnTag("llarp/BEncode.hpp", "failed to decode key ", k, " for entry in dict");
        
        return false;
      }
      read = true;
    }
    return true;
  }

  template < typename Int_t >
  bool
  BEncodeMaybeReadDictInt(const char* k, Int_t& i, bool& read,
                          llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, k))
    {
      if(!bencode_read_integer(buf, &i))
      {
        llarp::LogWarnTag("llarp/BEncode.hpp", "failed to decode key ", k, " for integer in dict");
        return false;
      }
      read = true;
    }
    return true;
  }

  template < typename Item_t >
  bool
  BEncodeMaybeReadVersion(const char* k, Item_t& item, uint64_t expect,
                          bool& read, llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, k))
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
    while(llarp_buffer_size_left(*buf) && *buf->cur != 'e')
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
    while(llarp_buffer_size_left(*buf) && *buf->cur != 'e')
    {
      T item;
      if(!item.BDecode(buf))
        return false;
      result.insert(item);
      /*
      if(!result.insert(item).second)
        return false;
      */
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

    virtual bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) = 0;

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
      if(k)
        return static_cast< IBEncodeMessage* >(r->user)->DecodeKey(*k,
                                                                   r->buffer);
      return true;
    }
  };

}  // namespace llarp

#endif