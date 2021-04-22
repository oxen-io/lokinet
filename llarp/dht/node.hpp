#pragma once

#include "key.hpp"
#include <llarp/router_contact.hpp>
#include <llarp/service/intro_set.hpp>
#include <utility>

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
      {}

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
      service::EncryptedIntroSet introset;

      Key_t ID;

      ISNode()
      {
        ID.Zero();
      }

      ISNode(service::EncryptedIntroSet other) : introset(std::move(other))
      {
        ID = Key_t(introset.derivedSigningKey.as_array());
      }

      util::StatusObject
      ExtractStatus() const
      {
        return introset.ExtractStatus();
      }

      bool
      operator<(const ISNode& other) const
      {
        return introset.signedAt < other.introset.signedAt;
      }
    };
  }  // namespace dht
}  // namespace llarp
