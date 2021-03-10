#ifndef LLARP_SERVICE_LOOKUP_HPP
#define LLARP_SERVICE_LOOKUP_HPP

#include <routing/message.hpp>
#include <service/intro_set.hpp>
#include <path/pathset.hpp>

#include <set>

namespace llarp
{
  // forward declare
  namespace path
  {
    struct Path;
  }

  namespace service
  {
    struct ILookupHolder;

    constexpr size_t MaxConcurrentLookups = size_t(16);

    struct IServiceLookup
    {
      IServiceLookup() = delete;
      virtual ~IServiceLookup() = default;

      /// handle lookup result for introsets
      virtual bool
      HandleIntrosetResponse(const std::set<EncryptedIntroSet>&)
      {
        return false;
      }

      /// handle lookup result for introsets
      virtual bool HandleNameResponse(std::optional<Address>)
      {
        return false;
      }

      virtual void
      HandleTimeout()
      {
        HandleIntrosetResponse({});
      }

      /// determine if this request has timed out
      bool
      IsTimedOut(llarp_time_t now, llarp_time_t timeout = 20s) const
      {
        if (now <= m_created)
          return false;
        return now - m_created > timeout;
      }

      /// build request message for service lookup
      virtual std::shared_ptr<routing::IMessage>
      BuildRequestMessage() = 0;

      /// build a new request message and send it via a path
      virtual bool
      SendRequestViaPath(path::Path_ptr p, AbstractRouter* r);

      ILookupHolder* m_parent;
      uint64_t txid;
      const std::string name;
      RouterID endpoint;

      util::StatusObject
      ExtractStatus() const
      {
        auto now = time_now_ms();
        util::StatusObject obj{
            {"txid", txid},
            {"endpoint", endpoint.ToHex()},
            {"name", name},
            {"timedOut", IsTimedOut(now)},
            {"createdAt", m_created.count()}};
        return obj;
      }

     protected:
      IServiceLookup(ILookupHolder* parent, uint64_t tx, std::string name);

      llarp_time_t m_created;
    };

    struct ILookupHolder
    {
      virtual void
      PutLookup(IServiceLookup* l, uint64_t txid) = 0;
    };

  }  // namespace service
}  // namespace llarp

#endif
