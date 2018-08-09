#ifndef LLARP_ENCCRYPTED_HPP
#define LLARP_ENCCRYPTED_HPP

#include <llarp/bencode.h>
#include <llarp/buffer.h>
#include <sodium.h>
#include <vector>

namespace llarp
{
  /// encrypted buffer base type
  struct Encrypted
  {
    Encrypted(Encrypted&&) = delete;
    Encrypted(const Encrypted& other);
    Encrypted();
    Encrypted(const byte_t* buf, size_t sz);
    Encrypted(size_t sz);
    ~Encrypted();

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, _data, _sz);
    }

    Encrypted&
    operator=(llarp_buffer_t buf)
    {
      if(_data)
        delete[] _data;
      _sz   = buf.sz;
      _data = new byte_t[_sz];
      memcpy(_data, buf.base, _sz);
      UpdateBuffer();
      return *this;
    }

    void
    Fill(byte_t fill)
    {
      size_t idx = 0;
      while(idx < _sz)
        _data[idx++] = fill;
    }

    void
    Randomize()
    {
      if(_data && _sz)
        randombytes(_data, _sz);
    }

    bool
    BDecode(llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz == 0)
        return false;
      if(_data)
        delete[] _data;
      _sz   = strbuf.sz;
      _data = new byte_t[_sz];
      memcpy(_data, strbuf.base, _sz);
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
      return _data;
    }

   protected:
    void
    UpdateBuffer()
    {
      m_Buffer.base = data();
      m_Buffer.cur  = data();
      m_Buffer.sz   = size();
    }
    byte_t* _data = nullptr;
    size_t _sz    = 0;
    llarp_buffer_t m_Buffer;
  };
}  // namespace llarp

#endif