#include <link/factory.hpp>
#include <iwp/iwp.hpp>
#include <utp/utp.hpp>

namespace llarp
{
  LinkFactory::LinkType
  LinkFactory::TypeFromName(string_view str)
  {
    if(str == "utp")
      return LinkType::eLinkUTP;
    if(str == "iwp")
      return LinkType::eLinkIWP;
    if(str == "mempipe")
      return LinkType::eLinkMempipe;
    return LinkType::eLinkUnknown;
  }

  std::string
  LinkFactory::NameFromType(LinkFactory::LinkType tp)
  {
    switch(tp)
    {
      case LinkType::eLinkUTP:
        return "utp";
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
    switch(tp)
    {
      case LinkType::eLinkUTP:
        if(permitInbound)
          return llarp::utp::NewInboundLink;
        return llarp::utp::NewOutboundLink;
      case LinkType::eLinkIWP:
        if(permitInbound)
          return llarp::iwp::NewInboundLink;
        return llarp::iwp::NewOutboundLink;
      default:
        return nullptr;
    }
  }
}  // namespace llarp