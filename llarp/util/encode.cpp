#include <util/encode.hpp>

#include <stdexcept>

namespace llarp
{
  int
  char2int(char input)
  {
    if (input >= '0' && input <= '9')
      return input - '0';
    if (input >= 'A' && input <= 'F')
      return input - 'A' + 10;
    if (input >= 'a' && input <= 'f')
      return input - 'a' + 10;
    return 0;
  }
}  // namespace llarp
