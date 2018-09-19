#ifndef LLARP_DHT_NODE_HPP
#define LLARP_DHT_NODE_HPP

#include <llarp/router_contact.hpp>
#include <llarp/dht/key.hpp>
#include <llarp/service/IntroSet.hpp>

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
        llarp::LogInfo("make ISNode with topic ", introset.topic.ToString());
      }
    };
  }  // namespace dht
}  // namespace llarp

#endif
