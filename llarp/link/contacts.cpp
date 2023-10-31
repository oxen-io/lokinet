#include "contacts.hpp"

#include <llarp/messages/dht.hpp>
#include <llarp/router/router.hpp>

namespace llarp
{
  Contacts::Contacts(const dht::Key_t& k, Router& r) : _local_key{k}, _router{r}
  {
    timer_keepalive = std::make_shared<int>(0);
    _router.loop()->call_every(1s, timer_keepalive, [this]() { on_clean_contacts(); });

    _rc_nodes = std::make_unique<dht::Bucket<dht::RCNode>>(_local_key, llarp::randint);
    _introset_nodes = std::make_unique<dht::Bucket<dht::ISNode>>(_local_key, llarp::randint);
  }

  std::optional<service::EncryptedIntroSet>
  Contacts::get_introset_by_location(const dht::Key_t& key) const
  {
    return _router.loop()->call_get([this, key]() -> std::optional<service::EncryptedIntroSet> {
      auto& introsets = _introset_nodes->nodes;

      if (auto itr = introsets.find(key); itr != introsets.end())
        return itr->second.introset;

      return std::nullopt;
    });
  }

  void
  Contacts::on_clean_contacts()
  {
    const auto now = llarp::time_now_ms();

    if (_rc_nodes)
    {
      auto& nodes = _rc_nodes->nodes;
      auto itr = nodes.begin();

      while (itr != nodes.end())
      {
        if (itr->second.rc.is_expired(now))
          itr = nodes.erase(itr);
        else
          ++itr;
      }
    }

    if (_introset_nodes)
    {
      auto& svcs = _introset_nodes->nodes;
      auto itr = svcs.begin();

      while (itr != svcs.end())
      {
        if (itr->second.introset.IsExpired(now))
          itr = svcs.erase(itr);
        else
          ++itr;
      }
    }
  }

  util::StatusObject
  Contacts::ExtractStatus() const
  {
    util::StatusObject obj{
        {"nodes", _rc_nodes->ExtractStatus()},
        {"services", _introset_nodes->ExtractStatus()},
        {"local_key", _local_key.ToHex()}};
    return obj;
  }

  bool
  Contacts::lookup_router(const RouterID& rid, std::function<void(oxen::quic::message)> func)
  {
    return _router.send_control_message(
        rid, "find_router", FindRouterMessage::serialize(rid, false, false), std::move(func));
  }

  void
  Contacts::put_rc_node_async(const dht::RCNode& val)
  {
    _router.loop()->call([this, val]() { _rc_nodes->PutNode(val); });
  }

  void
  Contacts::delete_rc_node_async(const dht::Key_t& val)
  {
    _router.loop()->call([this, val]() { _rc_nodes->DelNode(val); });
  }

}  // namespace llarp
