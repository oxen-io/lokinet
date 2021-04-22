#include "factory.hpp"
#include <llarp/iwp/iwp.hpp>

namespace llarp
{
  LinkFactory::LinkType
  LinkFactory::TypeFromName(std::string_view str)
  {
    if (str == "iwp")
      return LinkType::eLinkIWP;
    if (str == "mempipe")
      return LinkType::eLinkMempipe;
    return LinkType::eLinkUnknown;
  }

  std::string
  LinkFactory::NameFromType(LinkFactory::LinkType tp)
  {
    switch (tp)
    {
      case LinkType::eLinkIWP:
        return "iwp";
      case LinkType::eLinkMempipe:
        return "mempipe";
      default:
        return "unspec";
    }
  }

  LinkFactory::Factory
  LinkFactory::Obtain(LinkFactory::LinkType tp, bool permitInbound)
  {
    switch (tp)
    {
      case LinkType::eLinkIWP:
        if (permitInbound)
          return llarp::iwp::NewInboundLink;
        return llarp::iwp::NewOutboundLink;
      default:
        return nullptr;
    }
  }
}  // namespace llarp
