#pragma once

#include <llarp/dht/bucket.hpp>
#include <llarp/dht/node.hpp>

namespace llarp
{
  struct Router;

  /// This class mediates storage, retrieval, and functionality for the various types
  /// of contact information that needs to be stored locally by the link manager and
  /// router, like RouterContacts and introsets for example
  struct Contacts
  {
   private:
    // TODO: why was this a shared ptr in the original implementation? revisit this
    std::shared_ptr<int> timer_keepalive;
    const dht::Key_t& _local_key;
    Router& _router;
    std::atomic<bool> transit_allowed{false};

    // holds router contacts
    std::unique_ptr<dht::Bucket<dht::RCNode>> _rc_nodes;
    // holds introsets for remote services
    std::unique_ptr<dht::Bucket<dht::ISNode>> _introset_nodes;

   public:
    Contacts(const dht::Key_t& local, Router& r);

    /// Sets the value of transit_allowed to the value of `b`. Returns false if the
    /// value was already b, true otherwise
    bool
    set_transit_allowed(bool b)
    {
      return not transit_allowed.exchange(b) == b;
    }

    std::unordered_map<RouterID, std::function<void(const std::vector<RouterContact>&)>>
        pending_lookups;

    void
    on_clean_contacts();

    std::optional<service::EncryptedIntroSet>
    get_introset_by_location(const dht::Key_t& key) const;

    util::StatusObject
    extract_status() const;

    bool
    lookup_router(const RouterID&, std::function<void(const std::vector<RouterContact>&)>);

    void
    put_rc_node_async(const dht::RCNode& val);

    void
    delete_rc_node_async(const dht::Key_t& val);

    dht::Bucket<dht::RCNode>*
    rc_nodes() const
    {
      return _rc_nodes.get();
    }

    dht::Bucket<dht::ISNode>*
    services() const
    {
      return _introset_nodes.get();
    }

    Router*
    router() const
    {
      return &_router;
    }
  };

}  // namespace llarp
