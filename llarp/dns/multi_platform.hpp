#pragma once

#include "platform.hpp"
#include <vector>

namespace llarp::dns
{
  /// a collection of dns platforms that are tried in order when setting dns
  class Multi_Platform : public I_Platform
  {
    std::vector<std::unique_ptr<I_Platform>> m_Impls;

   public:
    /// add a platform to be owned
    void
    add_impl(std::unique_ptr<I_Platform> impl);

    /// try all owned platforms to set the resolver, throws if none of them work
    void
    set_resolver(std::string ifname, llarp::SockAddr dns, bool global) override;
  };
}  // namespace llarp::dns
