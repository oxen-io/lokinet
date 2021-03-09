#pragma once
#include <llarp/config/key_manager.hpp>
#include <string_view>
#include <functional>
#include <memory>

#include "server.hpp"

namespace llarp
{
  /// LinkFactory is responsible for returning std::functions that create the
  /// link layer types
  struct LinkFactory
  {
    enum class LinkType
    {
      eLinkUTP,
      eLinkIWP,
      eLinkMempipe,
      eLinkUnknown
    };

    using Factory = std::function<LinkLayer_ptr(
        std::shared_ptr<KeyManager>,
        GetRCFunc,
        LinkMessageHandler,
        SignBufferFunc,
        SessionEstablishedHandler,
        SessionRenegotiateHandler,
        TimeoutHandler,
        SessionClosedHandler,
        PumpDoneHandler)>;

    /// get link type by name string
    /// if invalid returns eLinkUnspec
    static LinkType
    TypeFromName(std::string_view name);

    /// turns a link type into a string representation
    static std::string
    NameFromType(LinkType t);

    /// obtain a link factory of a certain type
    static Factory
    Obtain(LinkType t, bool permitInbound);
  };

}  // namespace llarp
