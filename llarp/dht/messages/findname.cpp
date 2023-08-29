#include "findname.hpp"
#include <oxenc/bt_serialize.h>
#include <llarp/dht/context.hpp>
#include "gotname.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/rpc/lokid_rpc_client.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/routing/dht_message.hpp>

namespace llarp::dht
{
  FindNameMessage::FindNameMessage(const Key_t& from, Key_t namehash, uint64_t txid)
      : AbstractDHTMessage(from), NameHash(std::move(namehash)), TxID(txid)
  {}

  void
  FindNameMessage::bt_encode(oxenc::bt_dict_producer& btdp) const
  {
    try
    {
      btdp.append("A", "N");
      btdp.append("H", NameHash.ToView());
      btdp.append("T", TxID);
    }
    catch (...)
    {
      log::error(dht_cat, "Error: FindNameMessage failed to bt encode contents!");
    }
  }

  bool
  FindNameMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    if (key.startswith("H"))
    {
      return NameHash.BDecode(val);
    }
    if (key.startswith("T"))
    {
      return bencode_read_integer(val, &TxID);
    }
    return bencode_discard(val);
  }

  bool
  FindNameMessage::handle_message(struct llarp_dht_context* dht, std::vector<Ptr_t>& replies) const
  {
    (void)replies;
    auto r = dht->impl->GetRouter();
    if (pathID.IsZero() or not r->IsServiceNode())
      return false;
    r->RpcClient()->LookupLNSNameHash(NameHash, [r, pathID = pathID, TxID = TxID](auto maybe) {
      auto path = r->pathContext().GetPathForTransfer(pathID);
      if (path == nullptr)
        return;
      routing::DHTMessage msg;
      if (maybe.has_value())
      {
        msg.M.emplace_back(new GotNameMessage(dht::Key_t{}, TxID, *maybe));
      }
      else
      {
        msg.M.emplace_back(new GotNameMessage(dht::Key_t{}, TxID, service::EncryptedName{}));
      }
      path->SendRoutingMessage(msg, r);
    });
    return true;
  }

}  // namespace llarp::dht
