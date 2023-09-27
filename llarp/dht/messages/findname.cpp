#include "findname.hpp"
#include <oxenc/bt_serialize.h>
#include <llarp/dht/context.hpp>
#include "gotname.hpp"
#include <llarp/router/router.hpp>
#include <llarp/rpc/lokid_rpc_client.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/routing/path_dht_message.hpp>

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
  FindNameMessage::handle_message(
      AbstractDHTMessageHandler&, std::vector<std::unique_ptr<AbstractDHTMessage>>&) const
  {
    /* (void)replies;
    auto router = dht.GetRouter();
    if (pathID.IsZero() or not router->IsServiceNode())
      return false;
    router->rpc_client()->LookupLNSNameHash(
        NameHash, [router, pathID = pathID, TxID = TxID](auto maybe) {
          auto path = router->path_context().GetPathForTransfer(pathID);
          if (path == nullptr)
            return;
          routing::PathDHTMessage msg;
          if (maybe.has_value())
          {
            msg.dht_msgs.emplace_back(new GotNameMessage(dht::Key_t{}, TxID, *maybe));
          }
          else
          {
            msg.dht_msgs.emplace_back(
                new GotNameMessage(dht::Key_t{}, TxID, service::EncryptedName{}));
          }
          path->SendRoutingMessage(msg, router);
        }); */
    return true;
  }

}  // namespace llarp::dht
