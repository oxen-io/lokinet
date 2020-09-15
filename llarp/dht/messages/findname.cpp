#include <dht/messages/findname.hpp>
#include <lokimq/bt_serialize.h>
#include <dht/context.hpp>
#include <dht/messages/gotname.hpp>
#include <router/abstractrouter.hpp>
#include <rpc/lokid_rpc_client.hpp>
#include <path/path_context.hpp>
#include <routing/dht_message.hpp>

namespace llarp::dht
{
  FindNameMessage::FindNameMessage(const Key_t& from, Key_t namehash, uint64_t txid)
      : IMessage(from), NameHash(std::move(namehash)), TxID(txid)
  {
  }

  bool
  FindNameMessage::BEncode(llarp_buffer_t* buf) const
  {
    const auto data = lokimq::bt_serialize(
        lokimq::bt_dict{{"A", "N"sv},
                        {"H", std::string_view{(char*)NameHash.data(), NameHash.size()}},
                        {"T", TxID}});
    return buf->write(data.begin(), data.end());
  }

  bool
  FindNameMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    if (key == "H")
    {
      return NameHash.BDecode(val);
    }
    if (key == "T")
    {
      return bencode_read_integer(val, &TxID);
    }
    return bencode_discard(val);
  }

  bool
  FindNameMessage::HandleMessage(struct llarp_dht_context* dht, std::vector<Ptr_t>& replies) const
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
