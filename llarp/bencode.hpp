#ifndef LLARP_BENCODE_HPP
#define LLARP_BENCODE_HPP
#include <llarp/buffer.h>
#include <list>
#include <string>

namespace llarp
{
  template < typename ValType >
  bool
  BEncode(const ValType &t, llarp_buffer_t *buff);

  template < typename ValType >
  bool
  BDecode(ValType &t, llarp_buffer_t *buff);

  static bool
  bencodeDict(llarp_buffer_t *buff)
  {
    static uint8_t c = 'd';
    return llarp_buffer_write(buff, &c, 1);
  }

  static bool
  bencodeList(llarp_buffer_t *buff)
  {
    static uint8_t c = 'l';
    return llarp_buffer_write(buff, &c, 1);
  }

  static bool
  bencodeEnd(llarp_buffer_t *buff)
  {
    static char c = 'e';
    return llarp_buffer_write(buff, &c, 1);
  }

  static bool
  bencodeDictKey(llarp_buffer_t *buff, const std::string &key)
  {
    std::string kstr = std::to_string(key.size()) + ":" + key;
    return llarp_buffer_write(buff, kstr.c_str(), kstr.size());
  }

  template < typename IntType >
  static bool
  bencodeDict_Int(llarp_buffer_t *buff, const std::string &key, IntType i)
  {
    std::string istr = "i" + std::to_string(i) + "e";
    return bencodeDictKey(buff, key)
        && llarp_buffer_write(buff, istr.c_str(), istr.size());
  }

  static bool
  bencodeDict_Bytes(llarp_buffer_t *buff, const std::string &key,
                    const void *data, size_t sz)
  {
    std::string sz_str = std::to_string(sz) + ":";
    return bencodeDictKey(buff, key)
        && llarp_buffer_write(buff, sz_str.c_str(), sz_str.size())
        && llarp_buffer_write(buff, data, sz);
  }

  template < typename T >
  bool
  BEncode(const std::list< T > &l, llarp_buffer_t *buff)
  {
    if(bencodeList(buff))
    {
      for(const auto &itr : l)
        if(!BEncode(itr, buff))
          return false;
      return bencodeEnd(buff);
    }
    return false;
  }
}  // namespace llarp

#endif
