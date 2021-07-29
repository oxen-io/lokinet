#include <lokinet-dnsproxy.hpp>
#include <llarp/apple.hpp>
#include <oxenmq/oxenmq.h>
#include <llarp/util/logging/logger.hpp>
#include <thread>
#include <memory>

#include <llarp/util/buffer.hpp>
#include <llarp/dns/message.hpp>

struct DNSImpl
{
  oxenmq::OxenMQ m_MQ;
  std::optional<oxenmq::ConnectionID> m_Conn;

  explicit DNSImpl(oxenmq::address rpc)
  {
    m_MQ.start();
    m_MQ.connect_remote(
        rpc, [this](auto conn) { m_Conn = conn; }, nullptr);
  }

  bool
  ShouldHookFlow(NEAppProxyFlow* flow) const
  {
    LogInfo(NSObjectToString(flow));
    return true;
  }

  void
  RelayDNSData(NEAppProxyUDPFlow* flow, NWEndpoint* remote, NSData* data)
  {
    if (not m_Conn)
      return;
    auto view = DataAsStringView(data);

    llarp_buffer_t buf{view};
    llarp::dns::MessageHeader hdr{};
    if (not hdr.Decode(&buf))
      return;
    llarp::dns::Message msg{hdr};
    if (not msg.Decode(&buf))
      return;
    llarp::util::StatusObject request{
        {"qname", msg.questions[0].qname}, {"qtype", msg.questions[0].qtype}};
    m_MQ.request(
        *m_Conn,
        "llarp.dns_query",
        [flow, remote, msg = std::make_shared<llarp::dns::Message>(std::move(msg))](
            bool good, std::vector<std::string> parts) {
          auto closeHandler = [flow](NSError* err) {
            [flow closeWriteWithError:err];
            [flow closeReadWithError:err];
          };
          if (good and parts.size() == 1)
          {
            try
            {
              const auto obj = nlohmann::json::parse(parts[0]);
              const auto result = obj["result"];
              if (const auto itr = result.find("answers"); itr != result.end())
              {
                for (const auto& result : (*itr))
                {
                  llarp::dns::RR_RData_t rdata;
                  if (const auto data_itr = result.find("rdata"); data_itr != result.end())
                  {
                    const auto data = data_itr->get<std::string>();
                    rdata.resize(data.size());
                    std::copy_n(data.begin(), data.size(), rdata.begin());
                  }
                  else
                    continue;

                  msg->answers.emplace_back(
                      result["name"].get<std::string>(),
                      result["type"].get<llarp::dns::RRType_t>(),
                      rdata);
                }
              }
            }
            catch (std::exception& ex)
            {
              LogError("dns query failed: ", ex.what());
              return;
            }
            const auto buf = msg->ToBuffer();
            NSData* data = StringViewToData(
                std::string_view{reinterpret_cast<const char*>(buf.buf.get()), buf.sz});
            [flow writeDatagrams:@[data] sentByEndpoints:@[remote] completionHandler:closeHandler];
          }
          else
            closeHandler(nullptr);
        },
        request.dump());
  }

  void
  HandleUDPFlow(NEAppProxyUDPFlow* flow)
  {
    auto handler =
        [this, flow](
            NSArray<NSData*>* datagrams, NSArray<NWEndpoint*>* remoteEndpoints, NSError* error) {
          if (error)
            return;
          NSInteger num = [datagrams count];
          for (NSInteger idx = 0; idx < num; ++idx)
          {
            RelayDNSData(flow, [remoteEndpoints objectAtIndex:idx], [datagrams objectAtIndex:idx]);
          }
        };
    [flow readDatagramsWithCompletionHandler:handler];
  }
};

@implementation DNSProvider

- (void)startProxyWithOptions:(NSDictionary<NSString*, id>*)options
            completionHandler:(void (^)(NSError* error))completionHandler
{
  m_Impl = new DNSImpl{oxenmq::address{"tcp://127.0.0.1:1190"}};
  completionHandler(nil);
}

- (void)stopProxyWithReason:(NEProviderStopReason)reason
          completionHandler:(void (^)(void))completionHandler
{
  if (m_Impl)
  {
    delete m_Impl;
    m_Impl = nullptr;
  }
  completionHandler();
}

- (BOOL)handleNewFlow:(NEAppProxyFlow*)flow
{
  if (not [flow isKindOfClass:[NEAppProxyUDPFlow class]])
    return NO;
  if (m_Impl->ShouldHookFlow(flow))
  {
    NEAppProxyUDPFlow* udp = (NEAppProxyUDPFlow*)flow;
    auto handler = [impl = m_Impl, udp](NSError* err) {
      if (err)
        return;
      impl->HandleUDPFlow(udp);
    };
    [flow openWithLocalEndpoint:nil completionHandler:handler];
    return YES;
  }
  return NO;
}

@end
