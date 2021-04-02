#include "session.hpp"

namespace llarp
{
  namespace service
  {
    util::StatusObject
    Session::ExtractStatus() const
    {
      util::StatusObject obj{
          {"lastSend", to_json(lastSend)},
          {"lastRecv", to_json(lastRecv)},
          {"replyIntro", replyIntro.ExtractStatus()},
          {"remote", remote.Addr().ToString()},
          {"seqno", seqno},
          {"tx", messagesSend},
          {"rx", messagesRecv},
          {"intro", intro.ExtractStatus()}};
      return obj;
    }

    bool
    Session::IsExpired(llarp_time_t now, llarp_time_t lifetime) const
    {
      if (forever)
        return false;
      const auto lastUsed = std::max(lastSend, lastRecv);
      if(lastUsed == 0s)
        return intro.IsExpired(now);
      return now > lastUsed && (now - lastUsed > lifetime || intro.IsExpired(now));
    }

    void
    Session::TX()
    {
      messagesSend++;
      lastSend = time_now_ms();
    }

    void
    Session::RX()
    {
      messagesRecv++;
      lastRecv = time_now_ms();
    }

  }  // namespace service
}  // namespace llarp
