#ifndef LLARP_ENCCRYPTED_HPP
#define LLARP_ENCCRYPTED_HPP

#include <llarp/bencode.h>
#include <llarp/buffer.h>
#include <llarp/aligned.hpp>
#include <vector>
#include <stdexcept>

namespace llarp
{
  /// encrypted buffer base type
  struct Encrypted
  {
    Encrypted(Encrypted&& other);
    Encrypted(const Encrypted& other);
    Encrypted();
    Encrypted(const byte_t* buf, size_t sz);
    Encrypted(size_t sz);
    ~Encrypted();

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, data(), size());
    }

    bool
    operator==(const Encrypted& other) const
    {
      return size() == other.size()
          && memcmp(data(), other.data(), size()) == 0;
    }

    bool
    operator!=(const Encrypted& other) const
    {
      return !(*this == other);
    }

    Encrypted&
    operator=(const Encrypted& other)
    {
      return (*this) = other.Buffer();
    }

    Encrypted&
    operator=(const llarp_buffer_t& buf)
    {
      _data.resize(buf.sz);
      if(buf.sz)
      {
        memcpy(data(), buf.base, buf.sz);
      }
      UpdateBuffer();
      return *this;
    }

    void
    Fill(byte_t fill)
    {
      size_t _sz = size();
      size_t idx = 0;
      while(idx < _sz)
        _data[idx++] = fill;
    }

    void
    Randomize()
    {
      size_t _sz = size();
      if(_sz)
        randombytes(data(), _sz);
    }

    bool
    BDecode(llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz == 0)
        return false;
      _data.resize(strbuf.sz);
      memcpy(data(), strbuf.base, size());
      UpdateBuffer();
      return true;
    }

    llarp_buffer_t*
    Buffer()
    {
      return &m_Buffer;
    }

    llarp_buffer_t
    Buffer() const
    {
      return m_Buffer;
    }

    size_t
    size()
    {
      return _data.size();
    }

    size_t
    size() const
    {
      return _data.size();
    }

    byte_t*
    data()
    {
      return _data.data();
    }

    const byte_t*
    data() const
    {
      return _data.data();
    }

   protected:
    void
    UpdateBuffer()
    {
      m_Buffer.base = data();
      m_Buffer.cur  = data();
      m_Buffer.sz   = size();
    }
    std::vector< byte_t > _data;
    llarp_buffer_t m_Buffer;
  };
}  // namespace llarp

#endif
