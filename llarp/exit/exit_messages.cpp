#include "exit_messages.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp::routing
{
  bool
  ObtainExitMessage::Sign(const llarp::SecretKey& sk)
  {
    std::array<byte_t, 1024> tmp;
    llarp_buffer_t buf(tmp);
    pubkey = seckey_topublic(sk);
    sig.Zero();

    auto bte = bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->sign(sig, sk, buf);
  }

  bool
  ObtainExitMessage::Verify() const
  {
    std::array<byte_t, 1024> tmp;
    llarp_buffer_t buf(tmp);
    ObtainExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    buf.write(bte.begin(), bte.end());

    // rewind buffer
    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->verify(pubkey, buf, sig);
  }

  std::string
  ObtainExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "O");

      {
        auto subdict = btdp.append_dict("B");

        for (auto& b : blacklist_policy)
          b.bt_encode(subdict);
      }

      btdp.append("E", flag);
      btdp.append("I", pubkey.ToView());
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
      btdp.append("V", version);

      {
        auto subdict = btdp.append_dict("B");

        for (auto& w : whitelist_policy)
          w.bt_encode(subdict);
      }

      btdp.append("X", address_lifetime);
      btdp.append("Z", sig.ToView());
    }
    catch (...)
    {
      log::critical(route_cat, "Error: ObtainExitMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  ObtainExitMessage::decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictList("B", blacklist_policy, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("E", flag, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("I", pubkey, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("T", tx_id, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictList("W", whitelist_policy, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("X", address_lifetime, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  ObtainExitMessage::handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const
  {
    return h->HandleObtainExitMessage(*this, r);
  }

  std::string
  GrantExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "G");
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
      btdp.append("V", version);
      btdp.append("Y", nonce.ToView());
      btdp.append("Z", sig.ToView());
    }
    catch (...)
    {
      log::critical(route_cat, "Error: GrantExitMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  GrantExitMessage::decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("T", tx_id, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Y", nonce, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  GrantExitMessage::Verify(const llarp::PubKey& pk) const
  {
    std::array<byte_t, 512> tmp;
    llarp_buffer_t buf(tmp);
    GrantExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->verify(pk, buf, sig);
  }

  bool
  GrantExitMessage::Sign(const llarp::SecretKey& sk)
  {
    std::array<byte_t, 512> tmp;
    llarp_buffer_t buf(tmp);
    sig.Zero();
    nonce.Randomize();

    auto bte = bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->sign(sig, sk, buf);
  }

  bool
  GrantExitMessage::handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const
  {
    return h->HandleGrantExitMessage(*this, r);
  }

  std::string
  RejectExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "J");
      btdp.append("B", backoff_time);

      {
        auto subdict = btdp.append_dict("R");

        for (auto& b : blacklist_policy)
          b.bt_encode(subdict);
      }

      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
      btdp.append("V", version);
      btdp.append("Y", nonce.ToView());
      btdp.append("Z", sig.ToView());
    }
    catch (...)
    {
      log::critical(route_cat, "Error: RejectExitMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  RejectExitMessage::decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictInt("B", backoff_time, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictList("R", blacklist_policy, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("T", tx_id, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Y", nonce, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  RejectExitMessage::Sign(const llarp::SecretKey& sk)
  {
    std::array<byte_t, 512> tmp;
    llarp_buffer_t buf(tmp);
    sig.Zero();
    nonce.Randomize();

    auto bte = bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->sign(sig, sk, buf);
  }

  bool
  RejectExitMessage::Verify(const llarp::PubKey& pk) const
  {
    std::array<byte_t, 512> tmp;
    llarp_buffer_t buf(tmp);
    RejectExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->verify(pk, buf, sig);
  }

  bool
  RejectExitMessage::handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const
  {
    return h->HandleRejectExitMessage(*this, r);
  }

  std::string
  UpdateExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "V");
      btdp.append("P", path_id.ToView());
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
      btdp.append("V", version);
      btdp.append("Z", sig.ToView());
    }
    catch (...)
    {
      log::critical(route_cat, "Error: UpdateExitMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  UpdateExitMessage::decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("T", tx_id, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("P", path_id, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  UpdateExitMessage::Verify(const llarp::PubKey& pk) const

  {
    std::array<byte_t, 512> tmp;
    llarp_buffer_t buf(tmp);
    UpdateExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->verify(pk, buf, sig);
  }

  bool
  UpdateExitMessage::Sign(const llarp::SecretKey& sk)
  {
    std::array<byte_t, 512> tmp;
    llarp_buffer_t buf(tmp);
    nonce.Randomize();

    auto bte = bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->sign(sig, sk, buf);
  }

  bool
  UpdateExitMessage::handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const
  {
    return h->HandleUpdateExitMessage(*this, r);
  }

  std::string
  UpdateExitVerifyMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "V");
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
      btdp.append("V", version);
    }
    catch (...)
    {
      log::critical(route_cat, "Error: UpdateExitVerifyMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  UpdateExitVerifyMessage::decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("T", tx_id, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
      return false;
    return read;
  }

  bool
  UpdateExitVerifyMessage::handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const
  {
    return h->HandleUpdateExitVerifyMessage(*this, r);
  }

  std::string
  CloseExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "C");
      btdp.append("S", sequence_number);
      btdp.append("V", version);
      btdp.append("Y", nonce.ToView());
      btdp.append("Z", sig.ToView());
    }
    catch (...)
    {
      log::critical(route_cat, "Error: CloseExitMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  CloseExitMessage::decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Y", nonce, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  CloseExitMessage::Verify(const llarp::PubKey& pk) const
  {
    std::array<byte_t, 512> tmp;
    llarp_buffer_t buf(tmp);
    CloseExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->verify(pk, buf, sig);
  }

  bool
  CloseExitMessage::Sign(const llarp::SecretKey& sk)
  {
    std::array<byte_t, 512> tmp;
    llarp_buffer_t buf(tmp);
    sig.Zero();
    nonce.Randomize();

    auto bte = bt_encode();
    buf.write(bte.begin(), bte.end());

    buf.sz = buf.cur - buf.base;
    return CryptoManager::instance()->sign(sig, sk, buf);
  }

  bool
  CloseExitMessage::handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const
  {
    return h->HandleCloseExitMessage(*this, r);
  }
}  // namespace llarp::routing
