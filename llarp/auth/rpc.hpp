#pragma once

#include "auth.hpp"

#include <memory>
#include <string>
#include <unordered_set>

namespace oxenmq
{
    class OxenMQ;
    struct ConnectionID;
}  // namespace oxenmq

namespace llarp::auth
{
    struct RPCAuthPolicy final : public AuthPolicy
    {
        explicit RPCAuthPolicy(Router& r, std::string url, std::string method, oxenmq::OxenMQ& omq);

        ~RPCAuthPolicy() override;

        void start();

      private:
        const std::string _endpoint;
        const std::string _method;
        // const std::unordered_set<NetworkAddress> _whitelist;
        // const std::unordered_set<std::string> _static_tokens;

        oxenmq::OxenMQ& _omq;
        std::unique_ptr<oxenmq::ConnectionID> _omq_conn;
        std::unordered_set<session_tag> _pending_sessions;
    };

}  // namespace llarp::auth
