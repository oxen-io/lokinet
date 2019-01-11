#include <router_contact.hpp>

#include <constants/version.hpp>
#include <crypto.hpp>
#include <net/net.hpp>
#include <util/bencode.hpp>
#include <util/buffer.hpp>
#include <util/logger.hpp>
#include <util/mem.hpp>
#include <util/time.hpp>

#include <fstream>

namespace llarp
{
  NetID &
  NetID::DefaultValue()
  {
    static NetID defaultID(
        reinterpret_cast< const byte_t * >(Version::LLARP_NET_ID));
    return defaultID;
  }

  bool RouterContact::IgnoreBogons = false;

#ifdef TESTNET
  // 1 minute for testnet
  llarp_time_t RouterContact::Lifetime = 60 * 1000;
#else
  /// 1 day for real network
  llarp_time_t RouterContact::Lifetime = 24 * 60 * 60 * 1000;
#endif

  NetID::NetID(const byte_t *val) : AlignedBuffer< 8 >()
  {
    size_t len = strnlen(reinterpret_cast< const char * >(val), size());
    std::copy(val, val + len, begin());
  }

  NetID::NetID() : NetID(DefaultValue().data())
  {
  }

  bool
  NetID::operator==(const NetID &other) const
  {
    return ToString() == other.ToString();
  }

  std::string
  NetID::ToString() const
  {
    auto term = std::find(begin(), end(), '\0');
    return std::string(begin(), term);
  }

  bool
  NetID::BDecode(llarp_buffer_t *buf)
  {
    Zero();
    llarp_buffer_t strbuf;
    if(!bencode_read_string(buf, &strbuf))
      return false;
    if(strbuf.sz > size())
      return false;

    std::copy(strbuf.base, strbuf.base + strbuf.sz, begin());
    return true;
  }

  bool
  NetID::BEncode(llarp_buffer_t *buf) const
  {
    auto term = std::find(begin(), end(), '\0');
    return bencode_write_bytestring(buf, begin(), std::distance(begin(), term));
  }

  bool
  RouterContact::BEncode(llarp_buffer_t *buf) const
  {
    /* write dict begin */
    if(!bencode_start_dict(buf))
      return false;

    /* write ai if they exist */
    if(!bencode_write_bytestring(buf, "a", 1))
      return false;
    if(!BEncodeWriteList(addrs.begin(), addrs.end(), buf))
      return false;

    /* write netid */
    if(!bencode_write_bytestring(buf, "i", 1))
      return false;
    if(!netID.BEncode(buf))
      return false;
    /* write signing pubkey */
    if(!bencode_write_bytestring(buf, "k", 1))
      return false;
    if(!pubkey.BEncode(buf))
      return false;

    std::string nick = Nick();
    if(nick.size())
    {
      /* write nickname */
      if(!bencode_write_bytestring(buf, "n", 1))
        return false;
      if(!bencode_write_bytestring(buf, nick.c_str(), nick.size()))
        return false;
    }

    /* write encryption pubkey */
    if(!bencode_write_bytestring(buf, "p", 1))
      return false;
    if(!enckey.BEncode(buf))
      return false;

    /* write last updated */
    if(!bencode_write_bytestring(buf, "t", 1))
      return false;
    if(!bencode_write_uint64(buf, last_updated))
      return false;

    /* write version */
    if(!bencode_write_version_entry(buf))
      return false;

    /* write xi if they exist */
    if(!bencode_write_bytestring(buf, "x", 1))
      return false;
    if(!BEncodeWriteList(exits.begin(), exits.end(), buf))
      return false;

    /* write signature */
    if(!bencode_write_bytestring(buf, "z", 1))
      return false;
    if(!signature.BEncode(buf))
      return false;
    return bencode_end(buf);
  }

  void
  RouterContact::Clear()
  {
    addrs.clear();
    exits.clear();
    signature.Zero();
    nickname.Zero();
    enckey.Zero();
    pubkey.Zero();
    last_updated = 0;
  }

  bool
  RouterContact::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    bool read = false;
    if(!BEncodeMaybeReadDictList("a", addrs, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictEntry("i", netID, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictEntry("k", pubkey, read, key, buf))
      return false;

    if(llarp_buffer_eq(key, "n"))
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz > nickname.size())
        return false;
      nickname.Zero();
      std::copy(strbuf.base, strbuf.base + strbuf.sz, nickname.begin());
      return true;
    }

    if(!BEncodeMaybeReadDictEntry("p", enckey, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictInt("t", last_updated, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictList("x", exits, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictEntry("z", signature, read, key, buf))
      return false;

    return read;
  }

  bool
  RouterContact::IsPublicRouter() const
  {
    return addrs.size() > 0;
  }

  bool
  RouterContact::HasNick() const
  {
    return nickname[0] != 0;
  }

  void
  RouterContact::SetNick(const std::string &nick)
  {
    nickname.Zero();
    std::copy(nick.begin(),
              nick.begin() + std::min(nick.size(), nickname.size()),
              nickname.begin());
  }

  bool
  RouterContact::IsExpired(llarp_time_t now) const
  {
    auto expiresAt = last_updated + Lifetime;
    return now >= expiresAt;
  }

  bool
  RouterContact::ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const
  {
    if(IsExpired(now))
    {
      return true;
    }
    auto expiresAt = last_updated + Lifetime;
    return expiresAt - now <= dlt;
  }

  std::string
  RouterContact::Nick() const
  {
    auto term = std::find(nickname.begin(), nickname.end(), '\0');
    return std::string(nickname.begin(), term);
  }

  bool
  RouterContact::Sign(llarp::Crypto *crypto, const SecretKey &secretkey)
  {
    pubkey                  = llarp::seckey_topublic(secretkey);
    byte_t tmp[MAX_RC_SIZE] = {0};
    auto buf                = llarp::StackBuffer< decltype(tmp) >(tmp);
    signature.Zero();
    last_updated = time_now_ms();
    if(!BEncode(&buf))
      return false;
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    return crypto->sign(signature, secretkey, buf);
  }

  bool
  RouterContact::Verify(llarp::Crypto *crypto, llarp_time_t now) const
  {
    static const NetID networkNetID;
    if(netID != networkNetID)
    {
      llarp::LogError("netid missmatch: '", netID, "' != '", networkNetID, "'");
      return false;
    }
    if(IsExpired(now))
    {
      llarp::LogError("RC is expired");
      return false;
    }
    for(const auto &a : addrs)
    {
      if(IsBogon(a.ip) && !IgnoreBogons)
      {
        llarp::LogError("invalid address info: ", a);
        return false;
      }
    }
    for(const auto &exit : exits)
    {
      if(IsBogonRange(exit.address, exit.netmask))
        return false;
    }
    if(!VerifySignature(crypto))
    {
      llarp::LogError("invalid signature");
      return false;
    }
    return true;
  }

  bool
  RouterContact::VerifySignature(llarp::Crypto *crypto) const
  {
    RouterContact copy;
    copy = *this;
    copy.signature.Zero();
    byte_t tmp[MAX_RC_SIZE] = {0};
    auto buf                = llarp::StackBuffer< decltype(tmp) >(tmp);
    if(!copy.BEncode(&buf))
    {
      llarp::LogError("bencode failed");
      return false;
    }
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    return crypto->verify(pubkey, buf, signature);
  }

  bool
  RouterContact::Write(const char *fname) const
  {
    byte_t tmp[MAX_RC_SIZE] = {0};
    auto buf                = llarp::StackBuffer< decltype(tmp) >(tmp);
    if(!BEncode(&buf))
      return false;
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    {
      std::ofstream f;
      f.open(fname, std::ios::binary);
      if(!f.is_open())
        return false;
      f.write((char *)buf.base, buf.sz);
    }
    return true;
  }

  bool
  RouterContact::Read(const char *fname)
  {
    byte_t tmp[MAX_RC_SIZE] = {0};
    {
      std::ifstream f;
      f.open(fname, std::ios::binary);
      if(!f.is_open())
      {
        llarp::LogError("Failed to open ", fname);
        return false;
      }
      f.seekg(0, std::ios::end);
      auto l = f.tellg();
      f.seekg(0, std::ios::beg);
      f.read((char *)tmp, l);
    }
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    return BDecode(&buf);
  }

  RouterContact &
  RouterContact::operator=(const RouterContact &other)
  {
    addrs        = other.addrs;
    exits        = other.exits;
    signature    = other.signature;
    last_updated = other.last_updated;
    enckey       = other.enckey;
    pubkey       = other.pubkey;
    nickname     = other.nickname;
    version      = other.version;
    netID        = other.netID;
    return *this;
  }

}  // namespace llarp
