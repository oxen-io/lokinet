#include <llarp/bencode.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/version.h>
#include <llarp/crypto.hpp>
#include "buffer.hpp"
#include "logger.hpp"
#include "mem.hpp"

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

  bool
  RouterContact::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    bool read = false;
    if(!BEncodeMaybeReadDictList("a", addrs, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictEntry("k", pubkey, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictEntry("n", nickname, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictEntry("p", enckey, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictInt("u", last_updated, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictList("x", exits, read, key, buf))
      return false;

    if(!BEncodeMaybeReadDictEntry("z", signature, read, key, buf))
      return false;

    return read;
  }

}  // namespace llarp
