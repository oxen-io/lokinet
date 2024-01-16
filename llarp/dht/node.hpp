#pragma once

#include "key.hpp"

#include <llarp/router_contact.hpp>
#include <llarp/service/intro_set.hpp>

#include <utility>

namespace llarp::dht
{
  struct RCNode
  {
    RouterContact rc;
    Key_t ID;

    RCNode()
    {
      ID.Zero();
    }

    RCNode(const RouterContact& other) : rc(other), ID(other.router_id())
    {}

    util::StatusObject
    ExtractStatus() const
    {
      return rc.extract_status();
    }

    bool
    operator<(const RCNode& other) const
    {
      return rc.timestamp() < other.rc.timestamp();
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
}  // namespace llarp::dht
