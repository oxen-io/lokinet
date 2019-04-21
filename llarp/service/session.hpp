#ifndef LLARP_SERVICE_SESSION_HPP
#define LLARP_SERVICE_SESSION_HPP

#include <crypto/types.hpp>
#include <path/path.hpp>
#include <service/Info.hpp>
#include <service/Intro.hpp>
#include <util/status.hpp>
#include <util/types.hpp>

namespace llarp
{
  namespace service
  {
    struct Session
    {
      Introduction replyIntro;
      SharedSecret sharedKey;
      ServiceInfo remote;
      Introduction intro;
      llarp_time_t lastUsed = 0;
      uint64_t seqno        = 0;

      util::StatusObject
      ExtractStatus() const;

      bool
      IsExpired(llarp_time_t now,
                llarp_time_t lifetime = (path::default_lifetime * 2)) const
      {
        if(now <= lastUsed)
          return false;
        return now - lastUsed > lifetime;
      }
    };

  }  // namespace service

}  // namespace llarp

#endif
