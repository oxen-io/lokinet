#include <service/session.hpp>

namespace llarp
{
  namespace service
  {
    util::StatusObject
    Session::ExtractStatus() const
    {
      util::StatusObject obj{{"lastUsed", lastUsed},
                             {"replyIntro", replyIntro.ExtractStatus()},
                             {"remote", remote.Addr().ToString()},
                             {"seqno", seqno},
                             {"intro", intro.ExtractStatus()}};
      return obj;
    }

    bool
    Session::IsExpired(llarp_time_t now, llarp_time_t lifetime) const
    {
      if(now <= lastUsed)
        return intro.IsExpired(now);
      return now - lastUsed > lifetime || intro.IsExpired(now);
    }

  }  // namespace service
}  // namespace llarp
