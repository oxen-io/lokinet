#include "session.hpp"

namespace llarp
{
  namespace service
  {
    util::StatusObject
    Session::ExtractStatus() const
    {
      util::StatusObject obj{
          {"lastUsed", to_json(lastUsed)},
          {"replyIntro", replyIntro.ExtractStatus()},
          {"remote", remote.Addr().ToString()},
          {"seqno", seqno},
          {"intro", intro.ExtractStatus()}};
      return obj;
    }

    bool
    Session::IsExpired(llarp_time_t now, llarp_time_t lifetime) const
    {
      return now > lastUsed && (now - lastUsed > lifetime || intro.IsExpired(now));
    }

  }  // namespace service
}  // namespace llarp
