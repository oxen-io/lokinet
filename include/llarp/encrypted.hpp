#ifndef LLARP_ENCCRYPTED_HPP
#define LLARP_ENCCRYPTED_HPP

#include <llarp/buffer.h>

namespace llarp
{
  /// encrypted buffer base type
  struct Encrypted
  {
    Encrypted() = default;
    Encrypted(const byte_t* buf, size_t sz);
    Encrypted(size_t sz);
    ~Encrypted();

    llarp_buffer_t*
    Buffer()
    {
      return &m_Buffer;
    }

    byte_t* data = nullptr;
    size_t size  = 0;

   private:
    llarp_buffer_t m_Buffer;
  };
}

#endif