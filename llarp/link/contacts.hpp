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
    Router& _router;
    const dht::Key_t _local_key;

    // holds introsets for remote services
    std::unique_ptr<dht::Bucket<dht::ISNode>> _introset_nodes;

   public:
    Contacts(Router& r);

    std::optional<service::EncryptedIntroSet>
    get_introset_by_location(const dht::Key_t& key) const;

    // TODO: rename every ExtractStatus function to be uniformly snake cased
    util::StatusObject
    ExtractStatus() const;

    void
    put_intro(service::EncryptedIntroSet enc);

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
