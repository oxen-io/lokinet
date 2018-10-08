#include <llarp/bencode.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/version.h>
#include <llarp/crypto.hpp>
#include <llarp/time.h>
#include "buffer.hpp"
#include "logger.hpp"
#include "mem.hpp"

#include <fstream>

namespace llarp
{
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
    if(!bencode_write_bytestring(buf, "u", 1))
      return false;
    if(!bencode_write_uint64(buf, last_updated))
      return false;

    /* write version */
    if(!bencode_write_version_entry(buf))
      return false;

    /* write ai if they exist */
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
      memcpy(nickname.data(), strbuf.base, strbuf.sz);
      return true;
    }

    if(!BEncodeMaybeReadDictEntry("p", enckey, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictInt("v", version, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictInt("u", last_updated, read, key, buf))
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
    memcpy(nickname, nick.c_str(), std::min(nick.size(), nickname.size()));
  }

  std::string
  RouterContact::Nick() const
  {
    const char *n = (const char *)nickname.data();
    return std::string(n, strnlen(n, nickname.size()));
  }

  bool
  RouterContact::Sign(llarp_crypto *crypto, const SecretKey &secretkey)
  {
    pubkey                  = llarp::seckey_topublic(secretkey);
    byte_t tmp[MAX_RC_SIZE] = {0};
    auto buf                = llarp::StackBuffer< decltype(tmp) >(tmp);
    signature.Zero();
    last_updated = llarp_time_now_ms();
    if(!BEncode(&buf))
      return false;
    buf.sz  = buf.cur - buf.base;
    buf.cur = buf.base;
    return crypto->sign(signature, secretkey, buf);
  }

  bool
  RouterContact::VerifySignature(llarp_crypto *crypto) const
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
    return *this;
  }

}  // namespace llarp
