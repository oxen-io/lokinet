#include <llarp/encrypted_frame.hpp>

namespace llarp
{
  EncryptedFrame::EncryptedFrame() : m_Buf(nullptr), m_Sz(0)
  {
  }

  EncryptedFrame::EncryptedFrame(const byte_t* buf, size_t sz)
  {
    m_Sz  = sz;
    m_Buf = new byte_t[m_Sz];
    memcpy(m_Buf, buf, sz);
    m_Buffer.base = m_Buf;
    m_Buffer.cur  = m_Buf;
    m_Buffer.sz   = sz;
  }

  EncryptedFrame::~EncryptedFrame()
  {
    if(m_Buf)
      delete[] m_Buf;
  }

  bool
  EncryptedFrame::DecryptInPlace(const byte_t* ourSecretKey,
                                 llarp_crypto* crypto)
  {
    return false;
  }
}