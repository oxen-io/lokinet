#include <dht/taglookup.hpp>

#include <dht/context.hpp>
#include <dht/messages/gotintro.hpp>

namespace llarp
{
  namespace dht
  {
    bool
    TagLookup::Validate(const service::IntroSet &introset) const
    {
      if(!introset.Verify(parent->Now()))
      {
        llarp::LogWarn("got invalid introset from tag lookup");
        return false;
      }
      if(introset.topic != target)
      {
        llarp::LogWarn("got introset with missmatched topic in tag lookup");
        return false;
      }
      return true;
    }

    void
    TagLookup::Start(const TXOwner &peer)
    {
      parent->DHTSendTo(peer.node.as_array(),
                        new FindIntroMessage(target, peer.txid, R));
    }

    void
    TagLookup::SendReply()
    {
      std::set< service::IntroSet > found(valuesFound.begin(),
                                          valuesFound.end());

      // collect our local values if we haven't hit a limit
      if(found.size() < 2)
      {
        auto tags =
            parent->FindRandomIntroSetsWithTagExcluding(target, 1, found);
        std::copy(tags.begin(), tags.end(), std::inserter(found, found.end()));
      }
      std::vector< service::IntroSet > values(found.begin(), found.end());

      parent->DHTSendTo(whoasked.node.as_array(),
                        new GotIntroMessage(values, whoasked.txid));
    }
  }  // namespace dht
}  // namespace llarp
