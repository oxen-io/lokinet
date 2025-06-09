#include <lokinet.hpp>
#include <llarp.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>
#include <llarp/config/config.hpp>

#include <oxenc/base32z.h>

#include <future>

namespace
{
  static auto logcat = llarp::log::Cat("liblokinet");
}  // anonymous namespace

namespace lokinet
{
  Lokinet::Lokinet(const std::string& net_id) : context{std::make_shared<llarp::Context>()}, config{llarp::Config::make_embedded_config()}
  {
    config->router.net_id = net_id;
  }

  Lokinet::~Lokinet()
  {
    context->close_async();
    context->wait();
    run_thread.join();
  }

  void Lokinet::set_log_level(const std::string& level)
  {
    if (run_thread.joinable()) return;
    config->logging.level = llarp::log::level_from_string(level);
  }

  void Lokinet::run()
  {
    context->configure(config);
    const llarp::RuntimeOptions opts{};
    context->setup(opts);
    run_thread = std::thread{[this,opts=std::move(opts)](){context->run(opts);}};
  }

  uint16_t Lokinet::udp_session(const std::string& remote, uint16_t port)
  {
    auto maybe_netaddr = llarp::NetworkAddress::from_network_addr(remote);
    if (!maybe_netaddr) return 0;
    uint16_t bound_port = 0;
    std::promise<void> done;
    context->call_safe([&bound_port, context=this->context, netaddr=*maybe_netaddr, port, &done](){
        llarp::log::warning(logcat, "\nCreating session for udp test\n");
        context->router->session_endpoint()->initiate_remote_session(
            netaddr,
            [&bound_port, context, netaddr, port, &done](auto){
                llarp::log::warning(logcat, "\nCreated? session for udp test\n");
                if (auto session = context->router->session_endpoint()->get_session(netaddr); session) {
                  bound_port = session->setup_udp_mapping(port);
                  llarp::log::warning(logcat, "UDP Tunnel listening @ port {}", bound_port);
                }
                else
                  llarp::log::error(logcat, "UDP Tunnel something failed.");
                done.set_value();
            });
        });
    done.get_future().wait_for(10s);
    llarp::log::warning(logcat, "udp_session creation {}, bound port is {}", bound_port == 0 ? "failed" : "succeeded", bound_port);
    return bound_port;
  }
}  // namespace lokinet
