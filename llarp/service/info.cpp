#include <cassert>
#include <llarp/service/Info.hpp>
#include <llarp/service/address.hpp>
#include "buffer.hpp"
namespace llarp
{
  namespace service
  {
    ServiceInfo::ServiceInfo()
    {
      vanity.Zero();
    }

    ServiceInfo::~ServiceInfo()
    {
    }

    bool
    ServiceInfo::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("e", enckey, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("s", signkey, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("v", version, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("x", vanity, read, key, val))
        return false;
      return read;
    }

    bool
    ServiceInfo::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictEntry("e", enckey, buf))
        return false;
      if(!BEncodeWriteDictEntry("s", signkey, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "v", LLARP_PROTO_VERSION))
        return false;
      if(!vanity.IsZero())
      {
        if(!BEncodeWriteDictEntry("x", vanity, buf))
          return false;
      }
      return bencode_end(buf);
    }

    std::string
    ServiceInfo::Name() const
    {
      // TODO: caching addr
      Address addr;
      CalculateAddress(addr);
      return addr.ToString();
    }

    bool
    ServiceInfo::CalculateAddress(byte_t* addr) const
    {
      byte_t tmp[128];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      assert(BEncode(&buf));
      return crypto_generichash(addr, 32, buf.base, buf.cur - buf.base, nullptr,
                                0)
          != -1;
    }
  }  // namespace service
}  // namespace llarp