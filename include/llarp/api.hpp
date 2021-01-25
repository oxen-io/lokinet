#pragma once

#include <llarp.hpp>
#include <functional>
#include <optional>

namespace llarp::api
{
  class Endpoint;

  // an address we can send and recv packets to and from
  // copyable but non moveable
  class NetAddress
  {
    NetAddressImpl* const m_Impl;

   public:
    explicit NetAddress(NetAddressImpl*);

    /// copy constructor
    NetAddress(const NetAddress&);

    /// copy assignment
    NetAddress&
    operator=(const NetAddress&);

    ~NetAddress();

    /// get string representation of this address
    std::string
    ToString() const;

    std::ostream&
    operator<<(std::ostream& out) const
    {
      return out << ToString();
    }

    bool
    Equals(const NetAddress&) const;

    /// equality operator
    bool
    operator==(const NetAddress& other) const
    {
      return Equals(other);
    }

    /// inequality operator
    bool
    operator!=(const NetAddress& other) const
    {
      return not Equals(other);
    }
  };

  /// a type in charge of handling events on a stream
  class IStreamHandler
  {
   public:
    virtual ~IStreamHandler() = default;

    /// handles when we read data from the remote peer
    virtual void OnRecv(std::string_view) = 0;

    /// handles when we have established the stream with the remote peer
    /// does not fire for inbound connections
    virtual void
    OnConnected() = 0;
  };

  class StreamImpl;

  /// a stream oriented connection that receives and sends data in order
  class Stream
  {
    StreamImpl* const m_Impl;

   public:
    explicit Stream(StreamImpl* impl);
    ~Stream();

    /// attach a handler to this stream
    void
    AttachHandler(std::unique_ptr<IStreamHandler> handler);

    /// get remote address we are connected to
    /// throws if not connected
    NetAddress
    RemoteAddress() const;

    /// get remote port we are talking to
    /// throws if not connected
    uint16_t
    RemotePort() const;

    /// get local address we are on
    NetAddress
    LocalAddress() const;

    /// get local port we are on
    uint16_t
    LocalPort() const;

    /// send data to remote endpoint in an ordered fashion
    /// return true if the data was queued, returns false if the data was not queued and the
    /// operation would have blocked
    bool
    Send(std::string_view data);

    /// close the connection
    void
    Close();

    /// return true if this connection is closed
    bool
    IsClosed() const;
  };

  class StreamAcceptorImpl;

  /// base type for accepting inbound stream connections
  class StreamAcceptor
  {
    StreamAcceptorImpl* const m_Impl;

   protected:
    /// create a handler to put into the stream when we get an inbound stream
    virtual std::unique_ptr<IStreamHandler>
    CreateStreamHandler() = 0;

   public:
    explicit StreamAcceptor(std::shared_ptr<Endpoint> ep, uint16_t localPort);
    ~StreamAcceptor();

    /// start accepting connections
    void
    Listen();

    /// stop accepting connections and close all open connections
    void
    Close();

    /// handle when we get a new inbound connection
    virtual void
    HandleInboundConnection(std::shared_ptr<Stream> inbound) = 0;
  };

  class StreamConnectorImpl;

  class StreamConnector
  {
    StreamConnectorImpl* const m_Impl;

   public:
    /// construct a stream connector to try to connect from endpoint to remoteAddr on remotePort
    /// durring construction packets are sent out to the network
    /// calls OnConnected / OnConnectFailure when this connection attempt concludes after
    /// which this StreamConnector is safe to destruct
    explicit StreamConnector(
        std::shared_ptr<Endpoint> ep, NetAddress remoteAddr, uint16_t remotePort);
    ~StreamConnector();

    /// connection result type
    enum class FailType
    {
      eTimeout,
      eRefused,
      eReset
    };

    /// handle connection failure passes in why the connection failed
    virtual void OnConnectFailure(FailType) = 0;

    /// handle connection success
    virtual void OnConnected(std::shared<Stream>) = 0;
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
    HandlePacketFrom(NetAddress fromAddr, uint16_t fromPort, std::string_view data) = 0;

    /// close the socket forever
    void
    Close();
  };

  class EndpointPrivKeysImpl;

  /// persistent private key material
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
    /// constructs an endpoint with optionally provided endpoint keys
    /// if endpoint keys are not provided then ephemeral keys will be generated and used
    explicit Endpoint(std::unique_ptr<EndpointPrivKeys> keys = nullptr);
    ~Endpoint();

    // intrnal implementation
    EndpointImpl* const Impl;

    /// obtain our own network address
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
