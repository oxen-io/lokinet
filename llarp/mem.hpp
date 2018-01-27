#ifndef LLARP_MEM_HPP
#define LLARP_MEM_HPP
#include <llarp/mem.h>
#include <cmath>
namespace llarp
{
  template<typename T> 
  static constexpr size_t alignment()
  {
    return std::exp2(1+std::floor(std::log2(sizeof(T))));
  }
}

#endif
