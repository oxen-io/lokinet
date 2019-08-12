#ifndef LLARP_DHT_TX
#define LLARP_DHT_TX

#include <dht/key.hpp>
#include <dht/txowner.hpp>
#include <util/logger.hpp>
#include <util/status.hpp>

#include <set>
#include <vector>

namespace llarp
{
  namespace dht
  {
    struct AbstractContext;

    template < typename K, typename V >
    struct TX
    {
      K target;
      AbstractContext* parent;
      std::set< Key_t > peersAsked;
      std::vector< V > valuesFound;
      TXOwner whoasked;

      TX(const TXOwner& asker, const K& k, AbstractContext* p)
          : target(k), parent(p), whoasked(asker)
      {
      }

      virtual ~TX() = default;

      void
      OnFound(const Key_t& askedPeer, const V& value);

      /// return true if we want to persist this tx
      bool
      AskNextPeer(const Key_t& prevPeer, const std::unique_ptr< Key_t >& next);

      util::StatusObject
      ExtractStatus() const
      {
        util::StatusObject obj{{"whoasked", whoasked.ExtractStatus()},
                               {"target", target.ToHex()}};
        std::vector< util::StatusObject > foundObjs;
        std::transform(valuesFound.begin(), valuesFound.end(),
                       std::back_inserter(foundObjs),
                       [](const auto& item) -> util::StatusObject {
                         return item.ExtractStatus();
                       });

        obj.Put("found", foundObjs);
        std::vector< std::string > asked;
        std::transform(
            peersAsked.begin(), peersAsked.end(), std::back_inserter(asked),
            [](const auto& item) -> std::string { return item.ToHex(); });
        obj.Put("asked", asked);
        return obj;
      }

      virtual bool
      Validate(const V& value) const = 0;

      virtual void
      Start(const TXOwner& peer) = 0;

      virtual bool
      GetNextPeer(Key_t& next, const std::set< Key_t >& exclude) = 0;

      virtual void
      DoNextRequest(const Key_t& peer) = 0;

      virtual void
      SendReply() = 0;
    };

    template < typename K, typename V >
    inline void
    TX< K, V >::OnFound(const Key_t& askedPeer, const V& value)
    {
      peersAsked.insert(askedPeer);
      if(Validate(value))
      {
        valuesFound.push_back(value);
      }
    }

    template < typename K, typename V >
    inline bool
    TX< K, V >::AskNextPeer(const Key_t& prevPeer,
                            const std::unique_ptr< Key_t >& next)
    {
      peersAsked.insert(prevPeer);
      Key_t peer;
      if(next)
      {
        // explicit next peer provided
        peer = *next;
      }
      else if(GetNextPeer(peer, peersAsked))
      {
        const Key_t targetKey{target};
        if((prevPeer ^ targetKey) < (peer ^ targetKey))
        {
          // next peer is not closer
          llarp::LogDebug("next peer ", peer.SNode(), " is not closer to ",
                          target, " than ", prevPeer.SNode());
          return false;
        }
      }
      else
      {
        llarp::LogDebug("no more peers for request asking for ", target);
        return false;
      }
      peersAsked.insert(peer);
      DoNextRequest(peer);
      return true;
    }
  }  // namespace dht
}  // namespace llarp

#endif
