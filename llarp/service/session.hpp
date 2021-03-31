#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/path/path.hpp>
#include "info.hpp"
#include "intro.hpp"
#include <llarp/util/status.hpp>
#include <llarp/util/types.hpp>

namespace llarp
{
  namespace service
  {
    static constexpr auto SessionLifetime = path::default_lifetime * 2;

    struct Session
    {
      /// the intro we have
      Introduction replyIntro;
      SharedSecret sharedKey;
      ServiceInfo remote;
      /// the intro they have
      Introduction intro;
      /// the intro remoet last sent on
      Introduction lastInboundIntro;
      llarp_time_t lastUsed = 0s;
      uint64_t seqno = 0;
      bool inbound = false;
      bool forever = false;

      util::StatusObject
      ExtractStatus() const;
      bool
      IsExpired(llarp_time_t now, llarp_time_t lifetime = SessionLifetime) const;
    };

  }  // namespace service

}  // namespace llarp
