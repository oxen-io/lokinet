#ifndef LLARP_DHT_NODE_HPP
#define LLARP_DHT_NODE_HPP

#include <llarp/router_contact.h>
#include <llarp/dht/key.hpp>
#include <llarp/service/IntroSet.hpp>

namespace llarp
{
  namespace dht
  {
    struct RCNode
    {
      llarp_rc* rc;

      Key_t ID;

      RCNode() : rc(nullptr)
      {
        ID.Zero();
      }

      RCNode(llarp_rc* other) : rc(other)
      {
        ID = other->pubkey;
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
        other.A.CalculateAddress(ID);
      }
    };
  }
}

#endif