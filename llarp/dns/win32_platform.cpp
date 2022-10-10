#include "win32_platform.hpp"
#include <llarp/net/net.hpp>

namespace llarp::dns::win32
{
  void
  Platform::set_resolver(unsigned int index, llarp::SockAddr dns, bool)
  {
#ifdef _WIN32

    // clear any previous dns settings
    m_UndoDNS.clear();

    auto interfaces = m_Loop->Net_ptr()->AllNetworkInterfaces();
    // remove dns
    {
      std::vector<llarp::win32::OneShotExec> jobs;
      for (const auto& ent : interfaces)
      {
        if (ent.index == index)
          continue;
        jobs.emplace_back(
            "netsh.exe", fmt::format("interface ipv4 delete dns \"{}\" all", ent.name));
        jobs.emplace_back(
            "netsh.exe", fmt::format("interface ipv6 delete dns \"{}\" all", ent.name));
      }
    }
    // add new dns
    {
      std::vector<llarp::win32::OneShotExec> jobs;
      for (const auto& ent : interfaces)
      {
        if (ent.index == index)
          continue;
        jobs.emplace_back(
            "netsh.exe",
            fmt::format("interface ipv4 add dns \"{}\" {} validate=no", ent.name, dns.asIPv4()));
        jobs.emplace_back(
            "netsh.exe",
            fmt::format("interface ipv6 add dns \"{}\" {} validate=no", ent.name, dns.asIPv6()));
        m_UndoDNS.emplace_back("netsh.exe", fmt::format("", index));
      }
      m_UndoDNS.emplace_back("netsh.exe", "winsock reset");
    }
    // flush dns
    llarp::win32::Exec("ipconfig.exe", "/flushdns");

#endif
  }

}  // namespace llarp::dns::win32
