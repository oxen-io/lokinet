#include "gotname.hpp"
#include <oxenc/bt_serialize.h>
#include <llarp/dht/context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/path/path_context.hpp>

namespace llarp::dht
{
  constexpr size_t NameSizeLimit = 128;

  GotNameMessage::GotNameMessage(const Key_t& from, uint64_t txid, service::EncryptedName data)
      : AbstractDHTMessage(from), result(std::move(data)), TxID(txid)
  {
    if (result.ciphertext.size() > NameSizeLimit)
      throw std::invalid_argument("name data too big");
  }

  void
  GotNameMessage::bt_encode(oxenc::bt_dict_producer& btdp) const
  {
    try
    {
      btdp.append("A", "M");
      btdp.append("D", result.ciphertext);
      btdp.append("N", result.nonce.ToView());
      btdp.append("T", TxID);
    }
    catch (...)
    {
      log::error(dht_cat, "Error: GotNameMessage failed to bt encode contents!");
    }
  }

  bool
  GotNameMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    if (key.startswith("D"))
    {
      llarp_buffer_t str{};
      if (not bencode_read_string(val, &str))
        return false;
      if (str.sz > NameSizeLimit)
        return false;
      result.ciphertext.resize(str.sz);
      std::copy_n(str.cur, str.sz, result.ciphertext.data());
      return true;
    }
    if (key.startswith("N"))
    {
      return result.nonce.BDecode(val);
    }
    if (key.startswith("T"))
    {
      return bencode_read_integer(val, &TxID);
    }
    return bencode_discard(val);
  }

  bool
  GotNameMessage::handle_message(
      AbstractDHTMessageHandler& dht, std::vector<std::unique_ptr<AbstractDHTMessage>>&) const
  {
    auto pathset = dht.GetRouter()->pathContext().GetLocalPathSet(pathID);
    if (pathset == nullptr)
      return false;
    auto copy = std::make_shared<const GotNameMessage>(*this);
    return pathset->HandleGotNameMessage(copy);
  }

}  // namespace llarp::dht
