#pragma once

#include <llarp/constants/link_layer.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/bencode.h>
#include <llarp/util/buffer.hpp>
#include <llarp/util/mem.hpp>

#include <vector>
#include <stdexcept>

namespace llarp
{
  /// encrypted buffer base type
  template <size_t bufsz = MAX_LINK_MSG_SIZE>
  struct Encrypted
  {
    /// move construct
    // TODO: i want this to be explicitly deleted constructor
    Encrypted(Encrypted&& other)
    {
      _sz = std::move(other._sz);
      _buf = std::move(other._buf);
      UpdateBuffer();
    }

    /// copy construct
    Encrypted(const Encrypted& other) : Encrypted{other.data(), other.size()}
    {
      UpdateBuffer();
    }

    /// construct an emtpy frame of size 0
    Encrypted() : _buf{}, _sz{0}
    {
      UpdateBuffer();
    }

    /// clears out the entire buffer and sets size to zero
    /// updates owned mutable buffer
    void
    Clear()
    {
      std::fill(_buf.begin(), _buf.end(), 0);
      _sz = 0;
      UpdateBuffer();
    }

    /// copy construct from a byte pointer with size sz bytes
    Encrypted(const byte_t* buf, size_t sz)
    {
      if (sz <= bufsz)
      {
        _sz = sz;
        if (buf)
          memcpy(_buf.data(), buf, sz);
        else
          _buf.Zero();
      }
      else
        _sz = 0;
      UpdateBuffer();
    }

    /// construct zero'd buffer of size sz bytes
    Encrypted(size_t sz) : Encrypted{nullptr, sz}
    {}

    /// bencode to buffer as bytestring
    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, data(), _sz);
    }

    /// non constant time equals
    bool
    operator==(const Encrypted& other) const
    {
      return _sz == other._sz and memcmp(data(), other.data(), _sz) == 0;
    }

    bool
    operator!=(const Encrypted& other) const
    {
      return !(*this == other);
    }

    /// copy assignment
    /// calls Encrypted::operator=(const llarp_buffer_t &); under the hood
    Encrypted&
    operator=(const Encrypted& other)
    {
      return Encrypted::operator=(llarp_buffer_t(other));
    }

    /// operator overload for assigning a const llarp_buffer_t & to this encrypted frame
    /// copies full content of buf, sets encrypted frame as the same size as the copied buffer
    /// resets mutable owned buffer buffer
    Encrypted&
    operator=(const llarp_buffer_t& buf)
    {
      if (buf.sz <= _buf.size())
      {
        _sz = buf.sz;
        memcpy(_buf.data(), buf.base, _sz);
        UpdateBuffer();
      }
      return *this;
    }

    /// fill contents of EncryptedFrame with a single byte repeated
    /// only affects the current claimed size of the frame,
    /// if the current size is smaller than the max size the remaining is left unmodified
    void
    Fill(byte_t fill_byte)
    {
      std::fill(_buf.begin(), _buf.begin() + _sz, fill_byte);
    }

    /// randomize contents of EncryptedFrame
    /// if the current size is smaller than the max size the remaining is left unmodified
    void
    Randomize()
    {
      if (_sz)
        randombytes(_buf.data(), _sz);
    }

    /// decode from bencoded bytestring
    bool
    BDecode(llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if (not bencode_read_string(buf, &strbuf))
        return false;
      if (strbuf.sz > sizeof(_buf))
        return false;
      *this = strbuf;
      return true;
    }

    /// get a pointer to a mutable llarp_buffer_t owned by this EncryptedFrame
    llarp_buffer_t*
    Buffer()
    {
      return &m_Buffer;
    }

    /// size of buffer (const version)
    size_t
    size() const
    {
      return _sz;
    }

    /// get underlying pointer to data
    byte_t*
    data()
    {
      return _buf.data();
    }

    /// get underlying pointer to data (const version)
    const byte_t*
    data() const
    {
      return _buf.data();
    }

   protected:
    /// reset position and size of mutable owned buffer
    /// updates the size on the mutable buffer if the frame's size changed
    void
    UpdateBuffer()
    {
      m_Buffer.base = _buf.data();
      m_Buffer.cur = _buf.data();
      m_Buffer.sz = std::min(_sz, bufsz);
    }
    AlignedBuffer<bufsz> _buf;
    size_t _sz;
    llarp_buffer_t m_Buffer;
  };
}  // namespace llarp
