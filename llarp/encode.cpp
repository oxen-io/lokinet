#include <llarp/encode.hpp>
#include <stdexcept>

namespace llarp
{
  int
  char2int(char input)
  {
    if(input >= '0' && input <= '9')
      return input - '0';
    if(input >= 'A' && input <= 'F')
      return input - 'A' + 10;
    if(input >= 'a' && input <= 'f')
      return input - 'a' + 10;
    return 0;
  }

  bool
  HexDecode(const char* src, uint8_t* target, size_t sz)
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
