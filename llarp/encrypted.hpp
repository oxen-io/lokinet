#ifndef LLARP_ENCCRYPTED_HPP
#define LLARP_ENCCRYPTED_HPP

#include <aligned.hpp>
#include <bencode.h>
#include <buffer.h>
#include <mem.hpp>
#include <link_layer.hpp>

#include <vector>
#include <stdexcept>

namespace llarp
{
  /// encrypted buffer base type
  template < size_t bufsz = MAX_LINK_MSG_SIZE >
  struct Encrypted
  {
    Encrypted(Encrypted&& other)
    {
      _sz = std::move(other._sz);
      memcpy(_buf, other._buf, _sz);
      UpdateBuffer();
    }

    Encrypted(const Encrypted& other) : Encrypted(other.data(), other.size())
    {
      UpdateBuffer();
    }

    Encrypted()
    {
      Clear();
    }

    void
    Clear()
    {
      _sz = 0;
      UpdateBuffer();
    }

    Encrypted(const byte_t* buf, size_t sz)
    {
      if(sz <= bufsz)
      {
        _sz = sz;
        if(buf)
          memcpy(_buf, buf, sz);
        else
          llarp::Zero(_buf, sz);
      }
      else
        _sz = 0;
      UpdateBuffer();
    }

    Encrypted(size_t sz) : Encrypted(nullptr, sz)
    {
    }

    ~Encrypted()
    {
    }

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, _buf, _sz);
    }

    bool
    operator==(const Encrypted& other) const
    {
      return _sz == other._sz && memcmp(_buf, other._buf, _sz) == 0;
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
      if(buf.sz <= sizeof(_buf))
      {
        _sz = buf.sz;
        memcpy(_buf, buf.base, _sz);
      }
      UpdateBuffer();
      return *this;
    }

    void
    Fill(byte_t fill)
    {
      size_t idx = 0;
      while(idx < _sz)
        _buf[idx++] = fill;
    }

    void
    Randomize()
    {
      if(_sz)
        randombytes(_buf, _sz);
    }

    bool
    BDecode(llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz > sizeof(_buf))
        return false;
      _sz = strbuf.sz;
      if(_sz)
        memcpy(_buf, strbuf.base, _sz);
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
      return _sz;
    }

    size_t
    size() const
    {
      return _sz;
    }

    byte_t*
    data()
    {
      return _buf;
    }

    const byte_t*
    data() const
    {
      return _buf;
    }

   protected:
    void
    UpdateBuffer()
    {
      m_Buffer.base = _buf;
      m_Buffer.cur  = _buf;
      m_Buffer.sz   = _sz;
    }
    byte_t _buf[bufsz];
    size_t _sz;
    llarp_buffer_t m_Buffer;
  };  // namespace llarp
}  // namespace llarp

#endif
