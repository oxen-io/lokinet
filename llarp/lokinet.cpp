#include <llarp.hpp>
#include <llarp/address/address.hpp>
#include <llarp/config/config.hpp>
#include <llarp/net/id.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>

#include <lokinet.hpp>
#include <oxenc/base32z.h>

#include <exception>
#include <future>
#include <stdexcept>

using namespace std::literals;

namespace
{
    static auto logcat = llarp::log::Cat("liblokinet");
}  // anonymous namespace

namespace lokinet
{
    static auto make_embedded_context() { return std::make_unique<llarp::Context>(/*embedded=*/true); }

    Lokinet::Lokinet(std::string config, std::shared_ptr<oxen::quic::Loop> loop) : context{make_embedded_context()}
    {
        context->start(llarp::Config{llarp::config::Type::EmbeddedClient, std::move(config)}, loop);
    }

    Lokinet::Lokinet(path_ctor, const std::filesystem::path& config, std::shared_ptr<oxen::quic::Loop> loop)
        : context{make_embedded_context()}
    {
        ;
        context->start(llarp::Config{llarp::config::Type::EmbeddedClient, config}, loop);
    }

    Lokinet::Lokinet(Network n, std::shared_ptr<oxen::quic::Loop> loop) : context{make_embedded_context()}
    {
        llarp::Config conf{llarp::config::Type::EmbeddedClient};
        switch (n)
        {
            case Network::MAINNET:
                conf.router.net_id = llarp::NetID::MAINNET;
                break;
            case Network::TESTNET:
                conf.router.net_id = llarp::NetID::TESTNET;
                break;
            default:
                throw std::invalid_argument{"Unknown/unsupported network value passed to Lokinet constructor"};
        }
        context->start(std::move(conf), loop);
    }

    Lokinet::~Lokinet()
    {
        context->stop();
        context->wait();
    }

    void Lokinet::on_connected(std::function<void()> callback, bool persist) {
        context->router->on_connected(std::move(callback), persist);
    }

    void Lokinet::on_disconnected(std::function<void()> callback, bool persist) {
        context->router->on_disconnected(std::move(callback), persist);
    }

    void Lokinet::establish_udp(
        std::string_view remote,
        uint16_t port,
        std::function<void(tunnel_info info)> on_established,
        std::function<void(std::string errmsg)> failure)
    {
        // FIXME: check for valid ONS, and if so, we need to defer the lookup as well
        llarp::NetworkAddress netaddr;
        try
        {
            netaddr = llarp::NetworkAddress{remote};
        }
        catch (const std::exception& e)
        {
            failure("Invalid remote address: {}"_format(e.what()));
            return;
        }

        llarp::log::info(logcat, "Creating session for udp connection to {}", netaddr);
        context->router->session_endpoint().initiate_remote_session(
            netaddr,
            [&r = *context->router,
             port,
             netaddr,
             on_established = std::move(on_established),
             failure = std::move(failure)](llarp::session::Session& s) {
                if (!s.is_established())
                {
                    auto err = "Failed to establish remote session to {} for UDP tunnel[port={}]"_format(netaddr, port);
                    llarp::log::warning(logcat, "{}", err);
                    failure(std::move(err));
                    return;
                }

                // TODO FIXME: 1200 here is just a placeholder, we should be able to pick
                // something better!
                tunnel_info ti{.remote = netaddr.to_string(), .remote_port = port, .suggested_mtu = 1200};

                ti.local_port = s.setup_udp_mapping(port);
                llarp::log::info(
                    logcat,
                    "Session established to {}, with local port {} mapped to remote {}",
                    ti.remote,
                    ti.local_port,
                    ti.remote_port);

                on_established(std::move(ti));
            });
    }

    tunnel_info Lokinet::establish_udp_blocking(std::string_view remote, uint16_t port)
    {
        std::promise<tunnel_info> prom;
        auto fut = prom.get_future();
        establish_udp(
            remote,
            port,
            [&prom](tunnel_info info) { prom.set_value(std::move(info)); },
            [&prom](std::string err) {
                try
                {
                    throw std::runtime_error{err};
                }
                catch (...)
                {
                    prom.set_exception(std::current_exception());
                }
            });

        return fut.get();
    }

}  // namespace lokinet
