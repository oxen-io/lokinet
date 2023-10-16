#include "exit_messages.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp::routing
{
  bool
  ObtainExitMessage::Sign(const llarp::SecretKey& sk)
  {
    pubkey = seckey_topublic(sk);
    sig.Zero();

    auto bte = bt_encode();
    return CryptoManager::instance()->sign(
        sig, sk, reinterpret_cast<uint8_t*>(bte.data()), bte.size());
  }

  bool
  ObtainExitMessage::Verify() const
  {
    ObtainExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    return CryptoManager::instance()->verify(
        pubkey, reinterpret_cast<uint8_t*>(bte.data()), bte.size(), sig);
  }

  std::string
  ObtainExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("E", flag);
      btdp.append("I", pubkey.ToView());
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
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
    if (!BEncodeMaybeReadDictInt("E", flag, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("I", pubkey, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("T", tx_id, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  ObtainExitMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h->HandleObtainExitMessage(*this, r);
  }

  std::string
  GrantExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
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
    if (!BEncodeMaybeReadDictEntry("Y", nonce, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  GrantExitMessage::Verify(const llarp::PubKey& pk) const
  {
    GrantExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    return CryptoManager::instance()->verify(
        pk, reinterpret_cast<uint8_t*>(bte.data()), bte.size(), sig);
  }

  bool
  GrantExitMessage::Sign(const llarp::SecretKey& sk)
  {
    sig.Zero();
    nonce.Randomize();

    auto bte = bt_encode();
    return CryptoManager::instance()->sign(
        sig, sk, reinterpret_cast<uint8_t*>(bte.data()), bte.size());
  }

  bool
  GrantExitMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h->HandleGrantExitMessage(*this, r);
  }

  std::string
  RejectExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("B", backoff_time);
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
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
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictInt("T", tx_id, read, k, buf))
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
    sig.Zero();
    nonce.Randomize();

    auto bte = bt_encode();
    return CryptoManager::instance()->sign(
        sig, sk, reinterpret_cast<uint8_t*>(bte.data()), bte.size());
  }

  bool
  RejectExitMessage::Verify(const llarp::PubKey& pk) const
  {
    RejectExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    return CryptoManager::instance()->verify(
        pk, reinterpret_cast<uint8_t*>(bte.data()), bte.size(), sig);
  }

  bool
  RejectExitMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h->HandleRejectExitMessage(*this, r);
  }

  std::string
  UpdateExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("P", path_id.ToView());
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
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
    if (!BEncodeMaybeReadDictEntry("P", path_id, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  UpdateExitMessage::Verify(const llarp::PubKey& pk) const
  {
    UpdateExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    return CryptoManager::instance()->verify(
        pk, reinterpret_cast<uint8_t*>(bte.data()), bte.size(), sig);
  }

  bool
  UpdateExitMessage::Sign(const llarp::SecretKey& sk)
  {
    nonce.Randomize();

    auto bte = bt_encode();
    return CryptoManager::instance()->sign(
        sig, sk, reinterpret_cast<uint8_t*>(bte.data()), bte.size());
  }

  bool
  UpdateExitMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h->HandleUpdateExitMessage(*this, r);
  }

  std::string
  UpdateExitVerifyMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("S", sequence_number);
      btdp.append("T", tx_id);
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
    return read;
  }

  bool
  UpdateExitVerifyMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h->HandleUpdateExitVerifyMessage(*this, r);
  }

  std::string
  CloseExitMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("S", sequence_number);
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
    if (!BEncodeMaybeReadDictEntry("Y", nonce, read, k, buf))
      return false;
    if (!BEncodeMaybeReadDictEntry("Z", sig, read, k, buf))
      return false;
    return read;
  }

  bool
  CloseExitMessage::Verify(const llarp::PubKey& pk) const
  {
    CloseExitMessage copy;
    copy = *this;
    copy.sig.Zero();

    auto bte = copy.bt_encode();
    return CryptoManager::instance()->verify(
        pk, reinterpret_cast<uint8_t*>(bte.data()), bte.size(), sig);
  }

  bool
  CloseExitMessage::Sign(const llarp::SecretKey& sk)
  {
    sig.Zero();
    nonce.Randomize();

    auto bte = bt_encode();
    return CryptoManager::instance()->sign(
        sig, sk, reinterpret_cast<uint8_t*>(bte.data()), bte.size());
  }

  bool
  CloseExitMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h->HandleCloseExitMessage(*this, r);
  }
}  // namespace llarp::routing
