#include "rpc.hpp"

#include <llarp/router/router.hpp>

#include <oxenmq/oxenmq.h>

namespace llarp::auth
{
    static auto logcat = log::Cat("rpc.auth");

    RPCAuthPolicy::RPCAuthPolicy(Router& r, std::string url, std::string method, oxenmq::OxenMQ& omq)
        : AuthPolicy{r},
          _endpoint{std::move(url)},
          _method{std::move(method)},
          //   _whitelist{std::move(whitelist_addrs)},
          //   _static_tokens{std::move(whitelist_tokens)},
          _omq{omq}
    {
        if (_endpoint.empty() or _method.empty())
            throw std::invalid_argument{
                "RPC AuthPolicy must be initialized with an endpoint to query and a method to invoke!"};
    }

    RPCAuthPolicy::~RPCAuthPolicy() = default;

    void RPCAuthPolicy::start()
    {
        _omq.connect_remote(
            _endpoint,
            [this](oxenmq::ConnectionID c) {
                _omq_conn = std::make_unique<oxenmq::ConnectionID>(std::move(c));
                log::info(logcat, "OMQ connected to endpoint auth server");
            },
            [this](oxenmq::ConnectionID, std::string_view fail) {
                log::warning(logcat, "OMQ failed to connect to endpoint auth server: {}", fail);
                _router.loop()->call_later(1s, [this] { start(); });
            });
    }

}  // namespace llarp::auth
