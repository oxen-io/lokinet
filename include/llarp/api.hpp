#pragma once

#include <llarp.hpp>

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

  /// a UDPSocket for sending and recieving UDP packets across lokinet
  class UDPSocket
  {
    UDPSocketImpl* const m_Impl;

   public:
    /// construct a udp socket from an endpoint bound on local port
    explicit UDPSocket(std::shared_ptr<Endpoint> ep, uint16_t localPort);
    ~UDPSocket();

    /// send packet to remote address on remote port
    bool
    SendTo(std::shared_ptr<NetAddress> toAddr, uint16_t remotePort, std::string_view data);

    /// maybe read next UDP packet from the void
    std::optional<std::tuple<std::string, std::shared_ptr<NetAddress>>>
    RecvPacket();

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
    /// blocks until completed
    /// throws on failure
    std::shared_ptr<NetAdress>
    LookupAddress(std::string name);
  };

  /// make an application context that handles packets for a snapp endpoint
  std::unique_ptr<llarp::Context>
  MakeContext(std::shared_ptr<Endpoint> endpoint);
}  // namespace llarp::api
