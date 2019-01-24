#ifndef LLARP_DHT_TX
#define LLARP_DHT_TX

#include <dht/key.hpp>
#include <dht/txowner.hpp>
#include <util/logger.hpp>

#include <set>
#include <vector>

namespace llarp
{
  struct Router;

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

      virtual ~TX(){};

      virtual bool
      Validate(const V& value) const = 0;

      void
      OnFound(const Key_t askedPeer, const V& value);

      virtual void
      Start(const TXOwner& peer) = 0;

      virtual bool
      GetNextPeer(Key_t& next, const std::set< Key_t >& exclude) = 0;

      virtual void
      DoNextRequest(const Key_t& peer) = 0;

      /// return true if we want to persist this tx
      bool
      AskNextPeer(const Key_t& prevPeer, const std::unique_ptr< Key_t >& next);

      virtual void
      SendReply() = 0;
    };

    template < typename K, typename V >
    inline void
    TX< K, V >::OnFound(const Key_t askedPeer, const V& value)
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
      else if(!GetNextPeer(peer, peersAsked))
      {
        // no more peers
        llarp::LogInfo("no more peers for request asking for ", target);
        return false;
      }

      const Key_t targetKey{target};
      if((prevPeer ^ targetKey) < (peer ^ targetKey))
      {
        // next peer is not closer
        llarp::LogInfo("next peer ", peer, " is not closer to ", target,
                       " than ", prevPeer);
        return false;
      }
      else
      {
        peersAsked.insert(peer);
      }
      DoNextRequest(peer);
      return true;
    }
  }  // namespace dht
}  // namespace llarp

#endif
