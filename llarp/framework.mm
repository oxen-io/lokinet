#include <lokinet-extension.hpp>
#include <llarp.hpp>

#include <llarp/config/config.hpp>
#include <llarp/ev/vpn.hpp>
#include <llarp/util/thread/queue.hpp>

#include <string>

namespace llarp::apple
{
  struct FrameworkContext : public llarp::Context
  {
    
    explicit FrameworkContext(NEPacketTunnelProvider * tunnel);

    ~FrameworkContext() {}
    
    std::shared_ptr<vpn::Platform>
    makeVPNPlatform() override;

    void
    Start();
    
  private:
    NEPacketTunnelProvider * m_Tunnel;
    std::unique_ptr<std::thread> m_Runner;
  };  

  class VPNInterface final : public vpn::NetworkInterface
  {
    NEPacketTunnelProvider * m_Tunnel;

    static inline constexpr auto PacketQueueSize = 1024;
    
    thread::Queue<net::IPPacket> m_ReadQueue;

    void
    OfferReadPacket(NSData * data)
    {
      llarp::net::IPPacket pkt;
      const llarp_buffer_t buf{static_cast<const uint8_t *>(data.bytes), data.length};
      if(pkt.Load(buf))
        m_ReadQueue.tryPushBack(std::move(pkt));
    }

  public:
    explicit VPNInterface(NEPacketTunnelProvider * tunnel)
      : m_Tunnel{tunnel},
        m_ReadQueue{PacketQueueSize}
    {
      auto handler =
        [this](NSArray<NSData*> * packets, NSArray<NSNumber*> *)
        {
          NSUInteger num = [packets count];
          for(NSUInteger idx = 0; idx < num ; ++idx)
          {
            NSData * pkt = [packets objectAtIndex:idx];
            OfferReadPacket(pkt);
          }
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
      if(not m_ReadQueue.empty())
        pkt = m_ReadQueue.popFront();
      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      const sa_family_t fam = pkt.IsV6() ? AF_INET6 : AF_INET;
      const uint8_t * pktbuf = pkt.buf;
      const size_t pktsz = pkt.sz;
      NSData * datapkt = [NSData dataWithBytes:pktbuf length:pktsz];
      NEPacket * npkt = [[NEPacket alloc] initWithData:datapkt protocolFamily:fam];
      NSArray * pkts = @[npkt];
      return [m_Tunnel.packetFlow writePacketObjects:pkts];
    }
    
  };

  class VPNPlatform final : public vpn::Platform
  {
    NEPacketTunnelProvider * m_Tunnel;
  public:
    explicit VPNPlatform(NEPacketTunnelProvider * tunnel)
      : m_Tunnel{tunnel}
    {
    }
    
    std::shared_ptr<vpn::NetworkInterface>
    ObtainInterface(vpn::InterfaceInfo) override
    {
      return std::make_shared<VPNInterface>(m_Tunnel);
    }
  };

  
  FrameworkContext::FrameworkContext(NEPacketTunnelProvider * tunnel) :
    llarp::Context{},
    m_Tunnel{tunnel}
  {
  }

  void
  FrameworkContext::Start()
  {
    std::promise<void> result;

    m_Runner = std::make_unique<std::thread>(
      [&result, this]()
      {
        const RuntimeOptions opts{};
        try
        {
          Setup(opts);
          Configure(llarp::Config::NetworkExtensionConfig());
        }
        catch(std::exception & )
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
    return std::make_shared<VPNPlatform>(m_Tunnel);
  }
}


struct ContextWrapper
{
  std::shared_ptr<llarp::apple::FrameworkContext> m_Context;
public:
  explicit ContextWrapper(NEPacketTunnelProvider * tunnel) :
    m_Context{std::make_shared<llarp::apple::FrameworkContext>(tunnel)}
  {}

  void
  Start()
  {
    m_Context->Start();
  }

  void
  Stop()
  {
    m_Context->CloseAsync();
    m_Context->Wait();
  }
};
  


@implementation LLARPPacketTunnel

- (void)startTunnelWithOptions:(NSDictionary<NSString *,NSObject *> *)options completionHandler:(void (^)(NSError *error))completionHandler {
  m_Context = new ContextWrapper{self};
  m_Context->Start();
  completionHandler(nullptr);
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason 
completionHandler:(void (^)(void))completionHandler {
  if(m_Context)
  {
    m_Context->Stop();
    delete m_Context;
    m_Context = nullptr;
  }
  completionHandler();
}


@end
