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
      if(!introset.Verify(parent->Crypto(), parent->Now()))
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
      std::set< service::IntroSet > found;
      for(const auto &remoteTag : valuesFound)
      {
        found.insert(remoteTag);
      }
      // collect our local values if we haven't hit a limit
      if(found.size() < 2)
      {
        for(const auto &localTag :
            parent->FindRandomIntroSetsWithTagExcluding(target, 1, found))
        {
          found.insert(localTag);
        }
      }
      std::vector< service::IntroSet > values;
      for(const auto &introset : found)
      {
        values.push_back(introset);
      }
      parent->DHTSendTo(whoasked.node.as_array(),
                        new GotIntroMessage(values, whoasked.txid));
    }
  }  // namespace dht
}  // namespace llarp
