#include "link_intro.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/bencode.h>
#include <llarp/util/logging.hpp>

#include <oxenc/bt_producer.h>

namespace llarp
{
  bool
  LinkIntroMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key.startswith("a"))
    {
      llarp_buffer_t strbuf;
      if (!bencode_read_string(buf, &strbuf))
        return false;
      if (strbuf.sz != 1)
        return false;
      return *strbuf.cur == 'i';
    }
    if (key.startswith("n"))
    {
      if (nonce.BDecode(buf))
        return true;
      llarp::LogWarn("failed to decode nonce in LIM");
      return false;
    }
    if (key.startswith("p"))
    {
      return bencode_read_integer(buf, &session_period);
    }
    if (key.startswith("r"))
    {
      if (rc.BDecode(buf))
        return true;
      llarp::LogWarn("failed to decode RC in LIM");
      llarp::DumpBuffer(*buf);
      return false;
    }
    if (key.startswith("v"))
    {
      if (!bencode_read_integer(buf, &version))
        return false;
      if (version != llarp::constants::proto_version)
      {
        llarp::LogWarn(
            "llarp protocol version mismatch ", version, " != ", llarp::constants::proto_version);
        return false;
      }
      llarp::LogDebug("LIM version ", version);
      return true;
    }
    if (key.startswith("z"))
    {
      return sig.BDecode(buf);
    }

    llarp::LogWarn("invalid LIM key: ", *key.cur);
    return false;
  }

  std::string
  LinkIntroMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("a", "i");
      btdp.append("n", nonce.ToView());
      btdp.append("p", session_period);

      {
        auto subdict = btdp.append_list("r");
        rc.bt_encode_subdict(subdict);
      }

      btdp.append("v", llarp::constants::proto_version);
      btdp.append("z", sig.ToView());
    }
    catch (...)
    {
      log::critical(link_cat, "Error: LinkIntroMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  LinkIntroMessage::handle_message(Router* /*router*/) const
  {
    if (!verify())
      return false;
    return session->GotLIM(this);
  }

  void
  LinkIntroMessage::clear()
  {
    session_period = 0;
    nonce.Zero();
    rc.Clear();
    sig.Zero();
    version = 0;
  }

  bool
  LinkIntroMessage::sign(std::function<bool(Signature&, const llarp_buffer_t&)> signer)
  {
    sig.Zero();
    // need to keep this as a llarp_buffer_t for now, as all the crypto code expects
    // byte_t types -- fix this later
    std::array<byte_t, MAX_MSG_SIZE> tmp;
    llarp_buffer_t buf(tmp);

    auto bte = bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;

    return signer(sig, buf);
  }

  bool
  LinkIntroMessage::verify() const
  {
    LinkIntroMessage copy;
    copy = *this;
    copy.sig.Zero();

    // need to keep this as a llarp_buffer_t for now, as all the crypto code expects
    // byte_t types -- fix this later
    std::array<byte_t, MAX_MSG_SIZE> tmp;
    llarp_buffer_t buf(tmp);

    auto bte = copy.bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;

    // outer signature
    if (!CryptoManager::instance()->verify(rc.pubkey, buf, sig))
    {
      log::error(link_cat, "Error: outer signature failed!");
      return false;
    }
    // verify RC
    if (!rc.Verify(llarp::time_now_ms()))
    {
      log::error(link_cat, "Error: invalid RC in link intro!");
      return false;
    }
    return true;
  }

}  // namespace llarp
