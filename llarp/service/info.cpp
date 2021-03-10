#include "info.hpp"

#include <llarp/crypto/crypto.hpp>
#include "address.hpp"
#include <llarp/util/buffer.hpp>

#include <cassert>

#include <sodium/crypto_generichash.h>
#include <sodium/crypto_sign_ed25519.h>

namespace llarp
{
  namespace service
  {
    bool
    ServiceInfo::Verify(const llarp_buffer_t& payload, const Signature& sig) const
    {
      return CryptoManager::instance()->verify(signkey, payload, sig);
    }

    bool
    ServiceInfo::Update(
        const byte_t* sign, const byte_t* enc, const std::optional<VanityNonce>& nonce)
    {
      signkey = sign;
      enckey = enc;
      if (nonce)
      {
        vanity = *nonce;
      }
      return UpdateAddr();
    }

    bool
    ServiceInfo::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictEntry("e", enckey, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("s", signkey, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("v", version, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("x", vanity, read, key, val))
        return false;
      return read;
    }

    bool
    ServiceInfo::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictEntry("e", enckey, buf))
        return false;
      if (!BEncodeWriteDictEntry("s", signkey, buf))
        return false;
      if (!BEncodeWriteDictInt("v", LLARP_PROTO_VERSION, buf))
        return false;
      if (!vanity.IsZero())
      {
        if (!BEncodeWriteDictEntry("x", vanity, buf))
          return false;
      }
      return bencode_end(buf);
    }

    std::string
    ServiceInfo::Name() const
    {
      if (m_CachedAddr.IsZero())
      {
        Address addr;
        CalculateAddress(addr.as_array());
        return addr.ToString();
      }
      return m_CachedAddr.ToString();
    }

    bool
    ServiceInfo::CalculateAddress(std::array<byte_t, 32>& data) const
    {
      data = signkey.as_array();
      return true;
    }

    bool
    ServiceInfo::UpdateAddr()
    {
      if (m_CachedAddr.IsZero())
      {
        return CalculateAddress(m_CachedAddr.as_array());
      }
      return true;
    }

    std::ostream&
    ServiceInfo::print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printAttribute("e", enckey);
      printer.printAttribute("s", signkey);
      printer.printAttribute("v", version);
      printer.printAttribute("x", vanity);

      return stream;
    }

  }  // namespace service
}  // namespace llarp
