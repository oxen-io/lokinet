#include <messages/link_intro.hpp>

#include <crypto/crypto.hpp>
#include <router_contact.hpp>
#include <router/abstractrouter.hpp>
#include <util/bencode.h>
#include <util/logger.hpp>

namespace llarp
{
  LinkIntroMessage::~LinkIntroMessage()
  {
  }

  bool
  LinkIntroMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if(key == "a")
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz != 1)
        return false;
      return *strbuf.cur == 'i';
    }
    else if(key == "n")
    {
      if(N.BDecode(buf))
        return true;
      llarp::LogWarn("failed to decode nonce in LIM");
      return false;
    }
    else if(key == "p")
    {
      return bencode_read_integer(buf, &P);
    }
    else if(key == "r")
    {
      if(rc.BDecode(buf))
        return true;
      llarp::LogWarn("failed to decode RC in LIM");
      llarp::DumpBuffer(*buf);
      return false;
    }
    else if(key == "v")
    {
      if(!bencode_read_integer(buf, &version))
        return false;
      if(version != LLARP_PROTO_VERSION)
      {
        llarp::LogWarn("llarp protocol version missmatch ", version,
                       " != ", LLARP_PROTO_VERSION);
        return false;
      }
      llarp::LogDebug("LIM version ", version);
      return true;
    }
    else if(key == "z")
    {
      return Z.BDecode(buf);
    }
    else
    {
      llarp::LogWarn("invalid LIM key: ", *key.cur);
      return false;
    }
  }

  bool
  LinkIntroMessage::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;

    if(!bencode_write_bytestring(buf, "a", 1))
      return false;
    if(!bencode_write_bytestring(buf, "i", 1))
      return false;

    if(!bencode_write_bytestring(buf, "n", 1))
      return false;
    if(!N.BEncode(buf))
      return false;

    if(!bencode_write_bytestring(buf, "p", 1))
      return false;
    if(!bencode_write_uint64(buf, P))
      return false;

    if(!bencode_write_bytestring(buf, "r", 1))
      return false;
    if(!rc.BEncode(buf))
      return false;

    if(!bencode_write_version_entry(buf))
      return false;

    if(!bencode_write_bytestring(buf, "z", 1))
      return false;
    if(!Z.BEncode(buf))
      return false;

    return bencode_end(buf);
  }

  LinkIntroMessage&
  LinkIntroMessage::operator=(const LinkIntroMessage& msg)
  {
    version = msg.version;
    Z       = msg.Z;
    rc      = msg.rc;
    N       = msg.N;
    P       = msg.P;
    return *this;
  }

  bool
  LinkIntroMessage::HandleMessage(AbstractRouter* router) const
  {
    if(!Verify(router->crypto()))
      return false;
    return session->GotLIM(this);
  }

  void
  LinkIntroMessage::Clear()
  {
    P = 0;
    N.Zero();
    rc.Clear();
    Z.Zero();
  }

  bool
  LinkIntroMessage::Sign(
      std::function< bool(Signature&, const llarp_buffer_t&) > signer)
  {
    Z.Zero();
    std::array< byte_t, MaxSize > tmp;
    llarp_buffer_t buf(tmp);
    if(!BEncode(&buf))
      return false;
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    return signer(Z, buf);
  }

  bool
  LinkIntroMessage::Verify(llarp::Crypto* c) const
  {
    LinkIntroMessage copy;
    copy = *this;
    copy.Z.Zero();
    std::array< byte_t, MaxSize > tmp;
    llarp_buffer_t buf(tmp);
    if(!copy.BEncode(&buf))
      return false;
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    // outer signature
    if(!c->verify(rc.pubkey, buf, Z))
    {
      llarp::LogError("outer signature failure");
      return false;
    }
    // verify RC
    if(!rc.Verify(c, llarp::time_now_ms()))
    {
      llarp::LogError("invalid RC in link intro");
      return false;
    }
    return true;
  }

}  // namespace llarp
