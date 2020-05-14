#include <dht/taglookup.hpp>

#include <dht/context.hpp>
#include <dht/messages/gotintro.hpp>

namespace llarp
{
  namespace dht
  {
    bool
    TagLookup::Validate(const service::EncryptedIntroSet& introset) const
    {
      if (!introset.Verify(parent->Now()))
      {
        llarp::LogWarn("got invalid introset from tag lookup");
        return false;
      }
      if (not introset.topic.has_value())
        return false;
      if (introset.topic.value() != target)
      {
        llarp::LogWarn("got introset with mismatched topic in tag lookup");
        return false;
      }
      return true;
    }

    void
    TagLookup::Start(const TXOwner& peer)
    {
      parent->DHTSendTo(peer.node.as_array(), new FindIntroMessage(target, peer.txid));
    }

    void
    TagLookup::SendReply()
    {
      parent->DHTSendTo(whoasked.node.as_array(), new GotIntroMessage({}, whoasked.txid));
    }
  }  // namespace dht
}  // namespace llarp
