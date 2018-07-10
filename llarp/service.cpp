#include <llarp/service.hpp>
#include "buffer.hpp"
#include "router.hpp"

namespace llarp
{
  namespace service
  {
    std::string
    AddressToString(const Address& addr)
    {
      char tmp[(1 + 32) * 2] = {0};
      std::string str        = Base32Encode(addr, tmp);
      return str + ".loki";
    }

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

    bool
    ServiceInfo::CalculateAddress(Address& addr) const
    {
      byte_t tmp[128];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!BEncode(&buf))
        return false;
      return crypto_generichash(addr, addr.size(), buf.base, buf.cur - buf.base,
                                nullptr, 0)
          != -1;
    }

    IntroSet::~IntroSet()
    {
      if(W)
        delete W;
    }

    bool
    IntroSet::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("A", A, read, key, buf))
        return false;

      if(llarp_buffer_eq(key, "I"))
      {
        return BEncodeReadList(I, buf);
      }

      if(!BEncodeMaybeReadDictInt("V", version, read, key, buf))
        return false;

      if(!BEncodeMaybeReadDictEntry("Z", Z, read, key, buf))
        return false;

      return read;
    }

    bool
    IntroSet::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictEntry("a", A, buf))
        return false;
      // start introduction list
      if(!bencode_write_bytestring(buf, "i", 1))
        return false;
      if(!BEncodeWriteList(I.begin(), I.end(), buf))
        return false;
      // end introduction list

      // write version
      if(!BEncodeWriteDictInt(buf, "v", version))
        return false;
      if(W)
      {
        if(!BEncodeWriteDictEntry("w", *W, buf))
          return false;
      }
      if(!BEncodeWriteDictEntry("z", Z, buf))
        return false;

      return bencode_end(buf);
    }

    Introduction::~Introduction()
    {
    }

    bool
    Introduction::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("k", router, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("l", latency, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("p", pathID, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("x", expiresAt, read, key, buf))
        return false;
      return read;
    }

    bool
    Introduction::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      if(!BEncodeWriteDictEntry("k", router, buf))
        return false;
      if(latency)
      {
        if(!BEncodeWriteDictInt(buf, "l", latency))
          return false;
      }
      if(!BEncodeWriteDictEntry("p", pathID, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "v", version))
        return false;
      if(!BEncodeWriteDictInt(buf, "x", expiresAt))
        return false;
      return bencode_end(buf);
    }

    Identity::~Identity()
    {
    }

    bool
    Identity::BEncode(llarp_buffer_t* buf) const
    {
      /// TODO: implement me
      return false;
    }

    bool
    Identity::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      /// TODO: implement me
      return false;
    }

    void
    Identity::RegenerateKeys(llarp_crypto* crypto)
    {
      crypto->encryption_keygen(enckey);
      crypto->identity_keygen(signkey);
      pub.enckey  = llarp::seckey_topublic(enckey);
      pub.signkey = llarp::seckey_topublic(signkey);
    }

    bool
    Identity::EnsureKeys(const std::string& fname, llarp_crypto* c)
    {
      // TODO: implement me
      return false;
    }

    bool
    Identity::SignIntroSet(IntroSet& i, llarp_crypto* crypto) const
    {
      if(i.I.size() == 0)
        return false;
      i.A = pub;
      // zero out signature for signing process
      i.Z.Zero();
      byte_t tmp[MAX_INTROSET_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!i.BEncode(&buf))
        return false;
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      return crypto->sign(i.Z, signkey, buf);
    }

    bool
    IntroSet::VerifySignature(llarp_crypto* crypto) const
    {
      byte_t tmp[MAX_INTROSET_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      IntroSet copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      return crypto->verify(A.signkey, buf, Z);
    }

    Endpoint::Endpoint(const std::string& name, llarp_router* r)
        : m_Router(r), m_PathSet(llarp_pathbuilder_context_new(r, r->dht))
    {
    }

    bool
    Endpoint::SetOption(const std::string& k, const std::string& v)
    {
      if(k == "keyfile")
      {
        m_Keyfile = v;
        return true;
      }
      return false;
    }

    bool
    Endpoint::Start()
    {
      auto crypto = &m_Router->crypto;
      if(m_Keyfile.size())
      {
        if(!m_Identity.EnsureKeys(m_Keyfile, crypto))
          return false;
      }
      else
      {
        m_Identity.RegenerateKeys(crypto);
      }
      return true;
    }

    Endpoint::~Endpoint()
    {
      llarp_pathbuilder_context_free(m_PathSet);
    }

    bool
    Config::Load(const std::string& fname)
    {
      // TODO: implement me
      return false;
    }

  }  // namespace service
}  // namespace llarp
