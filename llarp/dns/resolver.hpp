#pragma once
#include <string>
#include <memory>
#include <llarp/net/sock_addr.hpp>
#include <llarp/util/logging.hpp>

#include <stdexcept>

namespace llarp
{
  struct AbstractRouter;
}

namespace llarp::dns
{
  /// sets dns settings in a platform dependant way
  class I_SystemSettings
  {
   public:
    virtual ~I_SystemSettings() = default;

    /// Attempts to set lokinet as the DNS server.
    /// throws if unsupported or fails.
    ///
    ///
    /// \param if_name -- the interface name to which we add the DNS servers, e.g. lokitun0.
    /// Typically tun_endpoint.GetIfName().
    /// \param dns -- the listening address of the lokinet DNS server
    /// \param global -- whether to set up lokinet for all DNS queries (true) or just .loki & .snode
    /// addresses (false).
    virtual void
    set_resolver(std::string if_name, llarp::SockAddr dns, bool global) = 0;
  };

  /// creates for the current platform
  std::shared_ptr<I_SystemSettings>
  MakeSystemSettings();

  /// compat wrapper
  inline bool
  set_resolver(std::string if_name, llarp::SockAddr dns, bool global)
  {
    try
    {
      if (auto settings = MakeSystemSettings())
      {
        settings->set_resolver(std::move(if_name), std::move(dns), global);
        return true;
      }
    }
    catch (std::exception& ex)
    {
      LogError("failed to set DNS: ", ex.what());
    }
    LogWarn("did not set dns");
    return false;
  }

}  // namespace llarp::dns
