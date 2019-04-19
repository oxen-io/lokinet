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
      RouterContact rc;
      Key_t ID;

      RCNode()
      {
        ID.Zero();
      }

      RCNode(const RouterContact& other) : rc(other), ID(other.pubkey)
      {
      }

      util::StatusObject
      ExtractStatus() const
      {
        return rc.ExtractStatus();
      }

      bool
      operator<(const RCNode& other) const
      {
        return rc.last_updated < other.rc.last_updated;
      }
    };

    struct ISNode
    {
      service::IntroSet introset;

      Key_t ID;

      ISNode()
      {
        ID.Zero();
      }

      ISNode(const service::IntroSet& other) : introset(other)
      {
        introset.A.CalculateAddress(ID.as_array());
      }

      util::StatusObject
      ExtractStatus() const
      {
        return introset.ExtractStatus();
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
