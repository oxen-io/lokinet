#ifndef LLARP_ENCODE_HPP
#define LLARP_ENCODE_HPP
#include <cstdlib>

namespace llarp
{
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

  int char2int(char input);
  void HexDecode(const char* src, uint8_t* target);
}

#endif
