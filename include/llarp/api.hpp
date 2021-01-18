#pragma once

#include <llarp.hpp>
#include <functional>
#include <optional>

namespace llarp::api
{
  class Endpoint;

  class NetAddressImpl;

  // an address we can send and recv packets to and from
  class NetAddress
  {
    NetAddressImpl* const m_Impl;

   public:
    explicit NetAddress(NetAddressImpl* impl);
    ~NetAddress();

    /// get string representation of this address
    std::string
    ToString() const;

    std::ostream&
    operator<<(std::ostream& out) const
    {
      return out << ToString();
    }
  };

  class UDPSocketImpl;

  /// base type for sending and recieving UDP packets across lokinet
  class UDPSocket
  {
    UDPSocketImpl* const m_Impl;

   public:
    /// construct a udp socket from an endpoint bound on local port
    explicit UDPSocket(std::shared_ptr<Endpoint> ep, uint16_t localPort);
    ~UDPSocket();

    /// send packet to remote address on remote port
    bool
    SendTo(NetAddress toAddr, uint16_t remotePort, std::string_view data);

    /// handle an inbound udp packet from the void
    /// implement in subtype
    virtual void
    HandlePacketFrom(NetAddress fromAddr, uint16_t fromPort, std::string_view data){};

    /// close the socket forever
    void
    Close();
  };

  class EndpointPrivKeysImpl;

  class EndpointPrivKeys
  {
    /// private implementation
    EndpointPrivKeysImpl* const Impl;
    /// load privkeys from existing file
    /// throws on failure
    explicit EndpointPrivKeys(std::string filename);
    ~EndpointPrivKeys();
  };

  class EndpointImpl;

  /// a snapp endpoint on lokinet
  class Endpoint
  {
   public:
    explicit Endpoint(std::unique_ptr<EndpointPrivKeys> keys = nullptr);
    ~Endpoint();

    // intrnal implementation
    EndpointImpl* const Impl;

    /// obtain our network address
    NetAddress
    GetOurAddress();

    /// look up address on the network
    /// calls resultHandler with nullptr on fail or with the found network address when found
    void
    LookupAddressAsync(
        std::string name, std::function<void(std::optional<NetAddress>)> resultHandler);
  };

  /// make an application context that handles packets for a snapp endpoint
  std::unique_ptr<llarp::Context>
  MakeContext(std::shared_ptr<Endpoint> endpoint);
}  // namespace llarp::api
