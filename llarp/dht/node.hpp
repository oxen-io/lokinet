#ifndef LLARP_DHT_NODE_HPP
#define LLARP_DHT_NODE_HPP

#include <dht/key.hpp>
#include <router_contact.hpp>
#include <service/IntroSet.hpp>

namespace llarp
{
  namespace dht
  {
    struct RCNode
    {
      llarp::RouterContact rc;

      Key_t ID;

      RCNode()
      {
        ID.Zero();
      }

      RCNode(const llarp::RouterContact& other)
      {
        rc = other;
        ID = other.pubkey.data();
      }

      bool
      operator<(const RCNode& other) const
      {
        return rc.last_updated < other.rc.last_updated;
      }
    };

    struct ISNode
    {
      llarp::service::IntroSet introset;

      Key_t ID;

      ISNode()
      {
        ID.Zero();
      }

      ISNode(const llarp::service::IntroSet& other)
      {
        introset = other;
        introset.A.CalculateAddress(ID);
      }

      bool
      operator<(const ISNode& other) const
      {
        return introset.T < other.introset.T;
      }
    };
  }  // namespace dht
}  // namespace llarp

#endif
