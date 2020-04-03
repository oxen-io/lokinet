#ifndef LLARP_ENCODE_HPP
#define LLARP_ENCODE_HPP
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace llarp
{
  // from  https://en.wikipedia.org/wiki/Base32#z-base-32
  static const char zbase32_alpha[] = {'y', 'b', 'n', 'd', 'r', 'f', 'g', '8', 'e', 'j', 'k',
                                       'm', 'c', 'p', 'q', 'x', 'o', 't', '1', 'u', 'w', 'i',
                                       's', 'z', 'a', '3', '4', '5', 'h', '7', '6', '9'};

  static const std::unordered_map<char, uint8_t> zbase32_reverse_alpha = {
      {'y', 0},  {'b', 1},  {'n', 2},  {'d', 3},  {'r', 4},  {'f', 5},  {'g', 6},  {'8', 7},
      {'e', 8},  {'j', 9},  {'k', 10}, {'m', 11}, {'c', 12}, {'p', 13}, {'q', 14}, {'x', 15},
      {'o', 16}, {'t', 17}, {'1', 18}, {'u', 19}, {'w', 20}, {'i', 21}, {'s', 22}, {'z', 23},
      {'a', 24}, {'3', 25}, {'4', 26}, {'5', 27}, {'h', 28}, {'7', 29}, {'6', 30}, {'9', 31}};

  template <int a, int b>
  static size_t
  DecodeSize(size_t sz)
  {
    auto d = div(sz, a);
    if (d.rem)
      d.quot++;
    return b * d.quot;
  }

  static size_t
  Base32DecodeSize(size_t sz)
  {
    return DecodeSize<5, 8>(sz);
  }

  template <typename Stack, typename V>
  bool
  Base32Decode(const Stack& stack, V& value)
  {
    int tmp = 0, bits = 0;
    size_t idx = 0;
    const size_t len = Base32DecodeSize(value.size());
    const size_t outLen = value.size();
    for (size_t i = 0; i < len; i++)
    {
      char ch = stack[i];
      if (ch)
      {
        auto itr = zbase32_reverse_alpha.find(ch);
        if (itr == zbase32_reverse_alpha.end())
          return false;
        ch = itr->second;
      }
      else
      {
        return idx == outLen;
      }
      tmp |= ch;
      bits += 5;
      if (bits >= 8)
      {
        if (idx >= outLen)
          return false;
        value[idx] = tmp >> (bits - 8);
        bits -= 8;
        idx++;
      }
      tmp <<= 5;
    }
    return idx == outLen;
  }

  /// adapted from i2pd
  template <typename V, typename Stack>
  const char*
  Base32Encode(const V& value, Stack& stack)
  {
    size_t ret = 0, pos = 1;
    int bits = 8;
    uint32_t tmp = value[0];
    size_t len = value.size();
    while (ret < sizeof(stack) && (bits > 0 || pos < len))
    {
      if (bits < 5)
      {
        if (pos < len)
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
      if (ret < sizeof(stack))
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
  template <typename V, typename Stack>
  const char*
  HexEncode(const V& value, Stack& stack)
  {
    size_t idx = 0;
    char* ptr = &stack[0];
    char* end = ptr + sizeof(stack);
    while (idx < value.size())
    {
      auto wrote = snprintf(ptr, end - ptr, "%.2x", value[idx]);
      if (wrote == -1)
        return nullptr;
      ptr += wrote;
      idx++;
    }
    *ptr = 0;
    return &stack[0];
  }

  int
  char2int(char input);

  template <typename OutputIt>
  bool
  HexDecode(const char* src, OutputIt target, size_t sz)
  {
    while (*src && src[1] && sz)
    {
      *(target++) = char2int(*src) * 16 + char2int(src[1]);
      src += 2;
      --sz;
    }
    return sz == 0;
  }

  static const char base64_table[] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
      'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
      'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
      'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

  template <typename OStream_t>
  void
  Base64Encode(OStream_t& out, const uint8_t* src, size_t len)
  {
    size_t i = 0;
    size_t j = 0;
    uint8_t buf[4] = {0};
    uint8_t tmp[3] = {0};
    while (len--)
    {
      tmp[i++] = *(src++);
      if (3 == i)
      {
        buf[0] = (tmp[0] & 0xfc) >> 2;
        buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
        buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
        buf[3] = tmp[2] & 0x3f;

        // encode
        for (i = 0; i < 4; ++i)
        {
          out << base64_table[buf[i]];
        }
        // reset
        i = 0;
      }
    }

    // remainder
    if (i > 0)
    {
      // fill `tmp' with `\0' at most 3 times
      for (j = i; j < 3; ++j)
      {
        tmp[j] = 0;
      }

      // encode remainder
      buf[0] = (tmp[0] & 0xfc) >> 2;
      buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
      buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
      buf[3] = tmp[2] & 0x3f;
      for (j = 0; (j < i + 1); ++j)
      {
        out << base64_table[buf[j]];
      }

      // pad
      while ((i++ < 3))
      {
        out << '=';
      }
    }
  }

}  // namespace llarp

#endif
