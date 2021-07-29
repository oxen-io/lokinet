#include <lokinet-extension.hpp>
#include <llarp.hpp>

#include <llarp/config/config.hpp>
#include <llarp/ev/vpn.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/util/logging/apple_logger.hpp>
#include <llarp/util/logging/buffer.hpp>
#include <llarp/net/ip_range.hpp>
#include <llarp/net/sock_addr.hpp>
#include <llarp/apple.hpp>

const llarp::SockAddr DefaultDNSBind{"127.0.0.1:1153"};
const llarp::SockAddr DefaultUpstreamDNS{"9.9.9.9:53"};

namespace llarp::apple
{
  struct FrameworkContext : public llarp::Context
  {
    explicit FrameworkContext(NEPacketTunnelProvider* tunnel);

    ~FrameworkContext()
    {}

    std::shared_ptr<vpn::Platform>
    makeVPNPlatform() override;

    void
    Start(std::string_view bootstrap);

   private:
    NEPacketTunnelProvider* const m_Tunnel;
    std::unique_ptr<std::thread> m_Runner;
  };

  class VPNInterface final : public vpn::NetworkInterface
  {
    NEPacketTunnelProvider* const m_Tunnel;

    static inline constexpr auto PacketQueueSize = 1024;

    thread::Queue<net::IPPacket> m_ReadQueue;

    void
    OfferReadPacket(NSData* data)
    {
      llarp::net::IPPacket pkt;
      const llarp_buffer_t buf{static_cast<const uint8_t*>(data.bytes), data.length};
      if (pkt.Load(buf))
      {
        m_ReadQueue.tryPushBack(std::move(pkt));
      }
      else
      {
        LogError("invalid IP packet: ", llarp::buffer_printer(DataAsStringView(data)));
      }
    }

   public:
    explicit VPNInterface(NEPacketTunnelProvider* tunnel, llarp::Context* context)
        : m_Tunnel{tunnel}, m_ReadQueue{PacketQueueSize}
    {
      context->loop->call_soon([this]() { Read(); });
    }

    void
    HandleReadEvent(NSArray<NSData*>* packets, NSArray<NSNumber*>* protos)
    {
      NSUInteger num = [packets count];
      for (NSUInteger idx = 0; idx < num; ++idx)
      {
        NSData* pkt = [packets objectAtIndex:idx];
        OfferReadPacket(pkt);
      }
      Read();
    }

    void
    Read()
    {
      auto handler = [this](NSArray<NSData*>* packets, NSArray<NSNumber*>* protos) {
        HandleReadEvent(packets, protos);
      };
      [m_Tunnel.packetFlow readPacketsWithCompletionHandler:handler];
    }

    int
    PollFD() const override
    {
      return -1;
    }

    std::string
    IfName() const override
    {
      return "";
    }

    net::IPPacket
    ReadNextPacket() override
    {
      net::IPPacket pkt{};
      if (not m_ReadQueue.empty())
        pkt = m_ReadQueue.popFront();
      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      NSNumber* fam = [NSNumber numberWithInteger:(pkt.IsV6() ? AF_INET6 : AF_INET)];
      void* pktbuf = pkt.buf;
      const size_t pktsz = pkt.sz;
      NSData* datapkt = [NSData dataWithBytesNoCopy:pktbuf length:pktsz];
      return [m_Tunnel.packetFlow writePackets:@[datapkt] withProtocols:@[fam]];
    }
  };

  class VPNPlatform final : public vpn::Platform
  {
    NEPacketTunnelProvider* const m_Tunnel;
    Context* const m_Context;

   public:
    explicit VPNPlatform(NEPacketTunnelProvider* tunnel, Context* context)
        : m_Tunnel{tunnel}, m_Context{context}
    {}

    std::shared_ptr<vpn::NetworkInterface> ObtainInterface(vpn::InterfaceInfo) override
    {
      return std::make_shared<VPNInterface>(m_Tunnel, m_Context);
    }
  };

  FrameworkContext::FrameworkContext(NEPacketTunnelProvider* tunnel)
      : llarp::Context{}, m_Tunnel{tunnel}
  {}

  void
  FrameworkContext::Start(std::string_view bootstrap)
  {
    std::promise<void> result;

    m_Runner = std::make_unique<std::thread>([&result, bootstrap = std::string{bootstrap}, this]() {
      const RuntimeOptions opts{};
      try
      {
        auto config = llarp::Config::NetworkExtensionConfig();
        config->bootstrap.files.emplace_back(bootstrap);
        config->dns.m_bind = DefaultDNSBind;
        config->dns.m_upstreamDNS.push_back(DefaultUpstreamDNS);
        Configure(std::move(config));
        Setup(opts);
      }
      catch (std::exception&)
      {
        result.set_exception(std::current_exception());
        return;
      }
      result.set_value();
      Run(opts);
    });

    auto ftr = result.get_future();
    ftr.get();
  }

  std::shared_ptr<vpn::Platform>
  FrameworkContext::makeVPNPlatform()
  {
    return std::make_shared<VPNPlatform>(m_Tunnel, this);
  }
}

struct ContextWrapper
{
  std::unique_ptr<llarp::apple::FrameworkContext> m_Context;

 public:
  explicit ContextWrapper(NEPacketTunnelProvider* tunnel)
      : m_Context{std::make_unique<llarp::apple::FrameworkContext>(tunnel)}
  {}

  void
  Start(std::string_view bootstrap)
  {
    llarp::LogContext::Instance().logStream.reset(new llarp::NSLogStream{});
    m_Context->Start(std::move(bootstrap));
  }

  void
  Stop()
  {
    m_Context->CloseAsync();
    m_Context->Wait();
  }
};

@implementation LLARPPacketTunnel

- (void)startTunnelWithOptions:(NSDictionary<NSString*, NSObject*>*)options
             completionHandler:(void (^)(NSError*))completionHandler
{
  llarp::huint32_t addr_{};
  llarp::huint32_t mask_{};
  if (auto maybe = llarp::FindFreeRange())
  {
    addr_ = llarp::net::TruncateV6(maybe->addr);
    mask_ = llarp::net::TruncateV6(maybe->netmask_bits);
  }
  NSString* addr = StringToNSString(addr_.ToString());
  NSString* mask = StringToNSString(mask_.ToString());

  NSBundle* main = [NSBundle mainBundle];
  NSString* res = [main pathForResource:@"bootstrap" ofType:@"signed"];
  NSData* path = [res dataUsingEncoding:NSUTF8StringEncoding];

  m_Context = new ContextWrapper{self};
  m_Context->Start(DataAsStringView(path));

  NEPacketTunnelNetworkSettings* settings =
      [[NEPacketTunnelNetworkSettings alloc] initWithTunnelRemoteAddress:@"127.0.0.1"];
  NEDNSSettings* dns = [[NEDNSSettings alloc] initWithServers:@[addr]];
  NEIPv4Settings* ipv4 = [[NEIPv4Settings alloc] initWithAddresses:@[addr]
                                                       subnetMasks:@[@"255.255.255.255"]];
  ipv4.includedRoutes = @[[[NEIPv4Route alloc] initWithDestinationAddress:addr subnetMask:mask]];
  settings.IPv4Settings = ipv4;
  settings.DNSSettings = dns;
  [self setTunnelNetworkSettings:settings completionHandler:completionHandler];
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)(void))completionHandler
{
  if (m_Context)
  {
    m_Context->Stop();
    delete m_Context;
    m_Context = nullptr;
  }
  completionHandler();
}

- (void)handleAppMessage:(NSData*)messageData
       completionHandler:(void (^)(NSData* responseData))completionHandler
{
  const auto data = DataAsStringView(messageData);
  completionHandler(StringViewToData("ok"));
}
@end
