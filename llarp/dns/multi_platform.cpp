#include "multi_platform.hpp"

namespace llarp::dns
{
  void
  Multi_Platform::add_impl(std::unique_ptr<I_Platform> impl)
  {
    m_Impls.emplace_back(std::move(impl));
  }

  void
  Multi_Platform::set_resolver(std::string ifname, llarp::SockAddr dns, bool global)
  {
    size_t fails{0};
    for (const auto& ptr : m_Impls)
    {
      try
      {
        ptr->set_resolver(ifname, dns, global);
      }
      catch (std::exception& ex)
      {
        LogWarn(ex.what());
        fails++;
      }
    }
    if (fails == m_Impls.size())
      throw std::runtime_error{"tried all ways to set resolver and failed"};
  }
}  // namespace llarp::dns
