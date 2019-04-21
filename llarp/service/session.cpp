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
    };
  }  // namespace service
}  // namespace llarp
