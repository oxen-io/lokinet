#ifndef LLARP_BENCODE_HPP
#define LLARP_BENCODE_HPP

#include <llarp/bencode.h>

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
  BEncodeWriteDictEntry(const char* k, const Obj_t& o, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1) && o.BEncode(buf);
  }

  template < typename Int_t >
  bool
  BEncodeWriteDictInt(llarp_buffer_t* buf, const char* k, const Int_t& i)
  {
    return bencode_write_bytestring(buf, k, 1) && bencode_write_uint64(buf, i);
  }

  template < typename Item_t >
  bool
  BEncodeRead(Item_t& item, llarp_buffer_t* buf);

  template < typename Item_t >
  bool
  BEncodeMaybeReadDictEntry(const char* k, Item_t& item, bool& read,
                            llarp_buffer_t key, llarp_buffer_t* buf)
  {
    llarp_buffer_t strbuf;
    if(llarp_buffer_eq(key, k))
    {
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(!item.BDecode(buf))
        return false;
      read = true;
      return true;
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
      return true;
    }
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

  template < typename List_t >
  bool
  BEncodeWriteDictList(const char* k, List_t& list, llarp_buffer_t* buf)
  {
    return bencode_write_bytestring(buf, k, 1)
        && BEncodeWriteList(list.begin(), list.end(), buf);
  }
}

#endif