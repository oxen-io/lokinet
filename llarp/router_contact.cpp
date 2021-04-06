#include "router_contact.hpp"

#include "constants/version.hpp"
#include "crypto/crypto.hpp"
#include "net/net.hpp"
#include "util/bencode.hpp"
#include "util/buffer.hpp"
#include "util/logging/logger.hpp"
#include "util/mem.hpp"
#include "util/printer.hpp"
#include "util/time.hpp"

#include <oxenmq/bt_serialize.h>

#include <fstream>
#include "util/fs.hpp"

namespace llarp
{
  NetID&
  NetID::DefaultValue()
  {
    static NetID defaultID(reinterpret_cast<const byte_t*>(llarp::DEFAULT_NETID));
    return defaultID;
  }

  bool RouterContact::BlockBogons = true;

#ifdef TESTNET
  // 1 minute for testnet
  llarp_time_t RouterContact::Lifetime = 1min;
#else
  /// 1 day for real network
  llarp_time_t RouterContact::Lifetime = 24h;
#endif
  /// an RC inserted long enough ago (4 hrs) is considered stale and is removed
  llarp_time_t RouterContact::StaleInsertionAge = 4h;
  /// update RCs shortly before they are about to expire
  llarp_time_t RouterContact::UpdateInterval = RouterContact::StaleInsertionAge - 5min;

  NetID::NetID(const byte_t* val)
  {
    const size_t len = strnlen(reinterpret_cast<const char*>(val), size());
    std::copy(val, val + len, begin());
  }

  NetID::NetID() : NetID(DefaultValue().data())
  {}

  bool
  NetID::operator==(const NetID& other) const
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
  NetID::BDecode(llarp_buffer_t* buf)
  {
    Zero();
    llarp_buffer_t strbuf;
    if (!bencode_read_string(buf, &strbuf))
      return false;

    if (strbuf.sz > size())
      return false;

    std::copy(strbuf.base, strbuf.base + strbuf.sz, begin());
    return true;
  }

  bool
  NetID::BEncode(llarp_buffer_t* buf) const
  {
    auto term = std::find(begin(), end(), '\0');
    return bencode_write_bytestring(buf, data(), std::distance(begin(), term));
  }

  bool
  RouterContact::BEncode(llarp_buffer_t* buf) const
  {
    if (version == 0)
      return BEncodeSignedSection(buf);
    else if (version == 1)
    {
      // TODO: heapless serialization for this in lokimq's bt serialization.
      if (not buf->writef("li1e%zu:", signature.size()))
        return false;
      if (not buf->write(signature.begin(), signature.end()))
        return false;
      if (not buf->write(signed_bt_dict.begin(), signed_bt_dict.end()))
        return false;
      if (not buf->writef("e"))
        return false;

      return true;
    }

    return false;
  }

  std::ostream&
  RouterContact::ToTXTRecord(std::ostream& out) const
  {
    for (const auto& addr : addrs)
    {
      out << "ai_addr=" << addr.toIpAddress() << "; ";
      out << "ai_pk=" << addr.pubkey.ToHex() << "; ";
    }
    out << "updated=" << last_updated.count() << "; ";
    out << "onion_pk=" << enckey.ToHex() << "; ";
    if (routerVersion.has_value())
      out << "router_version=" << routerVersion->ToString() << "; ";
    return out;
  }

  bool
  RouterContact::BEncodeSignedSection(llarp_buffer_t* buf) const
  {
    /* write dict begin */
    if (!bencode_start_dict(buf))
      return false;

    /* write ai if they exist */
    if (!bencode_write_bytestring(buf, "a", 1))
      return false;
    if (!BEncodeWriteList(addrs.begin(), addrs.end(), buf))
      return false;

    /* write netid */
    if (!bencode_write_bytestring(buf, "i", 1))
      return false;
    if (!netID.BEncode(buf))
      return false;
    /* write signing pubkey */
    if (!bencode_write_bytestring(buf, "k", 1))
      return false;
    if (!pubkey.BEncode(buf))
      return false;

    std::string nick = Nick();
    if (!nick.empty())
    {
      /* write nickname */
      if (!bencode_write_bytestring(buf, "n", 1))
      {
        return false;
      }
      if (!bencode_write_bytestring(buf, nick.c_str(), nick.size()))
      {
        return false;
      }
    }

    /* write encryption pubkey */
    if (!bencode_write_bytestring(buf, "p", 1))
      return false;
    if (!enckey.BEncode(buf))
      return false;

    // write router version if present
    if (routerVersion)
    {
      if (not BEncodeWriteDictEntry("r", *routerVersion, buf))
        return false;
    }

    if (version > 0)
    {
      // srv records if present
      if (not BEncodeWriteDictList("s", srvRecords, buf))
        return false;
    }
    /* write last updated */
    if (!bencode_write_bytestring(buf, "u", 1))
      return false;
    if (!bencode_write_uint64(buf, last_updated.count()))
      return false;

    /* write versions */
    if (!bencode_write_uint64_entry(buf, "v", 1, version))
      return false;

    if (serializeExit)
    {
      /* write xi if they exist */
      if (!bencode_write_bytestring(buf, "x", 1))
        return false;
      /* no exits anymore in RCs */
      const std::vector<AlignedBuffer<8>> exits{};
      if (!BEncodeWriteList(exits.begin(), exits.end(), buf))
        return false;
    }

    if (version == 0)
    {
      /* write signature */
      if (!bencode_write_bytestring(buf, "z", 1))
        return false;
      if (!signature.BEncode(buf))
        return false;
    }

    return bencode_end(buf);
  }

  void
  RouterContact::Clear()
  {
    addrs.clear();
    signature.Zero();
    nickname.Zero();
    enckey.Zero();
    pubkey.Zero();
    routerVersion = std::optional<RouterVersion>{};
    last_updated = 0s;
    srvRecords.clear();
    version = LLARP_PROTO_VERSION;
  }

  util::StatusObject
  RouterContact::ExtractStatus() const
  {
    util::StatusObject obj{
        {"lastUpdated", last_updated.count()},
        {"publicRouter", IsPublicRouter()},
        {"identity", pubkey.ToString()},
        {"addresses", addrs}};

    if (HasNick())
    {
      obj["nickname"] = Nick();
    }
    if (routerVersion)
    {
      obj["routerVersion"] = routerVersion->ToString();
    }
    std::vector<util::StatusObject> srv;
    for (const auto& record : srvRecords)
    {
      srv.emplace_back(record.ExtractStatus());
    }
    obj["srvRecords"] = srv;
    return obj;
  }

  bool
  RouterContact::BDecode(llarp_buffer_t* buf)
  {
    Clear();

    if (*buf->cur == 'd')  // old format
    {
      return DecodeVersion_0(buf);
    }
    else if (*buf->cur != 'l')  // if not dict, should be new format and start with list
    {
      return false;
    }

    try
    {
      std::string_view buf_view(reinterpret_cast<char*>(buf->cur), buf->size_left());
      oxenmq::bt_list_consumer btlist(buf_view);

      uint64_t outer_version = btlist.consume_integer<uint64_t>();

      if (outer_version == 1)
      {
        bool decode_result = DecodeVersion_1(btlist);

        // advance the llarp_buffer_t since lokimq serialization is unaware of it.
        buf->cur += btlist.current_buffer().data() - buf_view.data() + 1;

        return decode_result;
      }
      else
      {
        llarp::LogWarn("Received RouterContact with unkown version (", outer_version, ")");
        return false;
      }
    }
    catch (const std::exception& e)
    {
      llarp::LogDebug("RouterContact::BDecode failed, reason: ", e.what());
    }

    return false;
  }

  bool
  RouterContact::DecodeVersion_0(llarp_buffer_t* buf)
  {
    return bencode_decode_dict(*this, buf);
  }

  bool
  RouterContact::DecodeVersion_1(oxenmq::bt_list_consumer& btlist)
  {
    auto signature_string = btlist.consume_string_view();
    signed_bt_dict = btlist.consume_dict_data();

    if (not btlist.is_finished())
    {
      llarp::LogDebug("RouterContact serialized list too long for specified version.");
      return false;
    }

    llarp_buffer_t sigbuf(signature_string.data(), signature_string.size());
    if (not signature.FromBytestring(&sigbuf))
    {
      llarp::LogDebug("RouterContact serialized signature had invalid length.");
      return false;
    }

    llarp_buffer_t data_dict_buf(signed_bt_dict.data(), signed_bt_dict.size());
    return bencode_decode_dict(*this, &data_dict_buf);
  }

  bool
  RouterContact::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictList("a", addrs, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("i", netID, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("k", pubkey, read, key, buf))
      return false;

    if (key == "r")
    {
      RouterVersion r;
      if (not r.BDecode(buf))
        return false;
      routerVersion = r;
      return true;
    }

    if (key == "n")
    {
      llarp_buffer_t strbuf;
      if (!bencode_read_string(buf, &strbuf))
      {
        return false;
      }
      if (strbuf.sz > llarp::AlignedBuffer<(32)>::size())
      {
        return false;
      }
      nickname.Zero();
      std::copy(strbuf.base, strbuf.base + strbuf.sz, nickname.begin());
      return true;
    }

    if (not BEncodeMaybeReadDictList("s", srvRecords, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictEntry("p", enckey, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictInt("u", last_updated, read, key, buf))
      return false;

    if (!BEncodeMaybeReadDictInt("v", version, read, key, buf))
      return false;

    if (key == "x" and serializeExit)
    {
      return bencode_discard(buf);
    }

    if (!BEncodeMaybeReadDictEntry("z", signature, read, key, buf))
      return false;

    return read or bencode_discard(buf);
  }

  bool
  RouterContact::IsPublicRouter() const
  {
    if (not routerVersion)
      return false;
    return !addrs.empty();
  }

  bool
  RouterContact::HasNick() const
  {
    return nickname[0] != 0;
  }

  void
  RouterContact::SetNick(std::string_view nick)
  {
    nickname.Zero();
    std::copy(
        nick.begin(), nick.begin() + std::min(nick.size(), nickname.size()), nickname.begin());
  }

  bool
  RouterContact::IsExpired(llarp_time_t now) const
  {
    (void)now;
    return false;
    // return Age(now) >= Lifetime;
  }

  llarp_time_t
  RouterContact::TimeUntilExpires(llarp_time_t now) const
  {
    const auto expiresAt = last_updated + Lifetime;
    return now < expiresAt ? expiresAt - now : 0s;
  }

  llarp_time_t
  RouterContact::Age(llarp_time_t now) const
  {
    return now > last_updated ? now - last_updated : 0s;
  }

  bool
  RouterContact::ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const
  {
    return TimeUntilExpires(now) <= dlt;
  }

  std::string
  RouterContact::Nick() const
  {
    auto term = std::find(nickname.begin(), nickname.end(), '\0');
    return std::string(nickname.begin(), term);
  }

  bool
  RouterContact::Sign(const SecretKey& secretkey)
  {
    pubkey = llarp::seckey_topublic(secretkey);
    std::array<byte_t, MAX_RC_SIZE> tmp;
    llarp_buffer_t buf(tmp);
    signature.Zero();
    last_updated = time_now_ms();

    if (!BEncodeSignedSection(&buf))
    {
      return false;
    }
    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;

    signed_bt_dict = std::string(reinterpret_cast<char*>(buf.base), buf.sz);

    if (version == 0 or version == 1)
    {
      return CryptoManager::instance()->sign(signature, secretkey, buf);
    }

    return false;
  }

  bool
  RouterContact::Verify(llarp_time_t now, bool allowExpired) const
  {
    if (netID != NetID::DefaultValue())
    {
      llarp::LogError(
          "netid mismatch: '", netID, "' (theirs) != '", NetID::DefaultValue(), "' (ours)");
      return false;
    }
    if (IsExpired(now))
    {
      if (!allowExpired)
      {
        llarp::LogError("RC is expired");
        return false;
      }
      llarp::LogWarn("RC is expired");
    }
    for (const auto& a : addrs)
    {
      if (IsBogon(a.ip) && BlockBogons)
      {
        llarp::LogError("invalid address info: ", a);
        return false;
      }
    }
    if (!VerifySignature())
    {
      llarp::LogError("invalid signature: ", *this);
      return false;
    }
    return true;
  }

  bool
  RouterContact::VerifySignature() const
  {
    if (version == 0)
    {
      RouterContact copy;
      copy = *this;
      copy.signature.Zero();
      std::array<byte_t, MAX_RC_SIZE> tmp;
      llarp_buffer_t buf(tmp);
      if (!copy.BEncode(&buf))
      {
        llarp::LogError("bencode failed");
        return false;
      }
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      return CryptoManager::instance()->verify(pubkey, buf, signature);
    }
    /* else */
    if (version == 1)
    {
      llarp_buffer_t buf{signed_bt_dict};
      return CryptoManager::instance()->verify(pubkey, buf, signature);
    }

    return false;
  }

  bool
  RouterContact::Write(const fs::path& fname) const
  {
    std::array<byte_t, MAX_RC_SIZE> tmp;
    llarp_buffer_t buf(tmp);
    if (!BEncode(&buf))
    {
      return false;
    }
    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;
    auto f = llarp::util::OpenFileStream<std::ofstream>(fname, std::ios::binary);
    if (!f)
    {
      return false;
    }
    if (!f->is_open())
    {
      return false;
    }
    f->write((char*)buf.base, buf.sz);
    return true;
  }

  bool
  RouterContact::Read(const fs::path& fname)
  {
    std::array<byte_t, MAX_RC_SIZE> tmp;
    llarp_buffer_t buf(tmp);
    std::ifstream f;
    f.open(fname.string(), std::ios::binary);
    if (!f.is_open())
    {
      llarp::LogError("Failed to open ", fname);
      return false;
    }
    f.seekg(0, std::ios::end);
    auto l = f.tellg();
    if (l > static_cast<std::streamoff>(sizeof tmp))
    {
      return false;
    }
    f.seekg(0, std::ios::beg);
    f.read((char*)tmp.data(), l);
    return BDecode(&buf);
  }

  std::ostream&
  RouterContact::print(std::ostream& stream, int level, int spaces) const
  {
    Printer printer(stream, level, spaces);
    printer.printAttribute("k", pubkey);
    printer.printAttribute("updated", last_updated.count());
    printer.printAttribute("netid", netID);
    printer.printAttribute("v", version);
    printer.printAttribute("ai", addrs);
    printer.printAttribute("e", enckey);
    printer.printAttribute("z", signature);

    return stream;
  }

}  // namespace llarp
