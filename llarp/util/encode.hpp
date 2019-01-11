#ifndef LLARP_ENCODE_HPP
#define LLARP_ENCODE_HPP
#include <stdint.h>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace llarp
{
  // from  https://en.wikipedia.org/wiki/Base32#z-base-32
  static const char zbase32_alpha[] = {'y', 'b', 'n', 'd', 'r', 'f', 'g', '8',
                                       'e', 'j', 'k', 'm', 'c', 'p', 'q', 'x',
                                       'o', 't', '1', 'u', 'w', 'i', 's', 'z',
                                       'a', '3', '4', '5', 'h', '7', '6', '9'};

  static const std::unordered_map< char, uint8_t > zbase32_reverse_alpha = {
      {'y', 0},  {'b', 1},  {'n', 2},  {'d', 3},  {'r', 4},  {'f', 5},
      {'g', 6},  {'8', 7},  {'e', 8},  {'j', 9},  {'k', 10}, {'m', 11},
      {'c', 12}, {'p', 13}, {'q', 14}, {'x', 15}, {'o', 16}, {'t', 17},
      {'1', 18}, {'u', 19}, {'w', 20}, {'i', 21}, {'s', 22}, {'z', 23},
      {'a', 24}, {'3', 25}, {'4', 26}, {'5', 27}, {'h', 28}, {'7', 29},
      {'6', 30}, {'9', 31}};

  template < int a, int b >
  static size_t
  DecodeSize(size_t sz)
  {
    auto d = div(sz, a);
    if(d.rem)
      d.quot++;
    return b * d.quot;
  }

  template < typename Stack, typename V >
  bool
  Base32Decode(const Stack& stack, V& value)
  {
    int tmp = 0, bits = 0;
    size_t ret    = 0;
    size_t len    = DecodeSize< 5, 8 >(value.size());
    size_t outLen = value.size();
    for(size_t i = 0; i < len; i++)
    {
      char ch = stack[i];
      if(ch)
      {
        auto itr = zbase32_reverse_alpha.find(ch);
        if(itr == zbase32_reverse_alpha.end())
          return false;
        ch = itr->second;
      }
      else
      {
        return ret == outLen;
      }
      tmp |= ch;
      bits += 5;
      if(bits >= 8)
      {
        if(ret >= outLen)
          return false;
        value[ret] = tmp >> (bits - 8);
        bits -= 8;
        ret++;
      }
      tmp <<= 5;
    }
    return true;
  }

  /// adapted from i2pd
  template < typename V, typename Stack >
  const char*
  Base32Encode(const V& value, Stack& stack)
  {
    size_t ret = 0, pos = 1;
    int bits = 8, tmp = value[0];
    size_t len = value.size();
    while(ret < sizeof(stack) && (bits > 0 || pos < len))
    {
      if(bits < 5)
      {
        if(pos < len)
        {
          tmp <<= 8;
          tmp |= value[pos] & 0xFF;
          pos++;
          bits += 8;
        }
        else  // last byte
        {
          tmp <<= (5 - bits);
          bits = 5;
        }
      }

      bits -= 5;
      int ind = (tmp >> bits) & 0x1F;
      if(ret < sizeof(stack))
      {
        stack[ret] = zbase32_alpha[ind];
        ret++;
      }
      else
        return nullptr;
    }
    return &stack[0];
  }

  /// encode V as hex to stack
  /// null terminate
  /// return pointer to base of stack buffer on success otherwise returns
  /// nullptr
  template < typename V, typename Stack >
  const char*
  HexEncode(const V& value, Stack& stack)
  {
    size_t idx = 0;
    char* ptr  = &stack[0];
    char* end  = ptr + sizeof(stack);
    while(idx < value.size())
    {
      auto wrote = snprintf(ptr, end - ptr, "%.2x", value[idx]);
      if(wrote == -1)
        return nullptr;
      ptr += wrote;
      idx++;
    }
    *ptr = 0;
    return &stack[0];
  }

  int
  char2int(char input);

  template < typename OutputIt >
  bool
  HexDecode(const char* src, OutputIt target, size_t sz)
  {
    while(*src && src[1] && sz)
    {
      *(target++) = char2int(*src) * 16 + char2int(src[1]);
      src += 2;
      --sz;
    }
    return sz == 0;
  }

}  // namespace llarp

#endif
