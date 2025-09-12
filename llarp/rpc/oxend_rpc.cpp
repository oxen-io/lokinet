#include "oxend_rpc.hpp"

#include <llarp/nodedb.hpp>
#include <llarp/router/router.hpp>
#include <llarp/util/logging.hpp>

#include <nlohmann/json.hpp>
#include <oxenc/hex.h>

#include <exception>
#include <stdexcept>

namespace llarp::rpc
{
    static auto logcat = log::Cat("rpc.oxend");

    OxendRPC::OxendRPC(oxenmq::OxenMQ& omq, Router& r) : _omq{omq}, _router{r}
    {
        // new block handler
        _omq.add_category("notify", oxenmq::Access{oxenmq::AuthLevel::none})
            .add_command("block", [this](oxenmq::Message& m) { handle_new_block(m); });

        // TODO: proper auth here
        auto lokidCategory = _omq.add_category("lokid", oxenmq::Access{oxenmq::AuthLevel::none});
        _is_updating_list = false;
    }

    void OxendRPC::connect_async(oxenmq::address url)
    {
        if (not _router.is_service_node)
        {
            throw std::runtime_error("we cannot talk to lokid while not a service node");
        }

        log::info(logcat, "RPC client connecting to oxend at {}", url.full_address());

        _conn = _omq.connect_remote(
            url,
            [](oxenmq::ConnectionID) {},
            [this, url](oxenmq::ConnectionID, std::string_view f) {
                log::info(logcat, "Failed to connect to oxend at {}", f);
                _router.loop.call([this, url]() { connect_async(url); });
            });
    }

    void OxendRPC::command(std::string_view cmd)
    {
        log::debug(logcat, "Oxend command: {}", cmd);
        _omq.send(*_conn, std::move(cmd));
    }

    void OxendRPC::handle_new_block(oxenmq::Message& msg)
    {
        if (msg.data.size() != 2)
        {
            log::error(
                logcat,
                "Received invalid new block notification with {} parts (expected 2); not updating service node list!",
                msg.data.size());

            return;  // bail
        }
        try
        {
            _block_height = std::stoll(std::string{msg.data[0]});
        }
        catch (std::exception& ex)
        {
            log::error(logcat, "Bad block height: {}", ex.what());

            return;  // bail
        }

        log::trace(logcat, "new block at height {}", _block_height);
        update_service_node_list();
    }

    void OxendRPC::update_service_node_list(std::shared_ptr<std::promise<void>> on_updated)
    {
        if (_is_updating_list.exchange(true))
        {
            assert(!on_updated);  // When using a promise it should be the first call
            return;               // update already in progress
        }

        nlohmann::json req{{"fields", {"pubkey_ed25519", "block_hash"}}};
        if (!_last_hash_update.empty())
            req["poll_block_hash"] = _last_hash_update;

        request(
            "rpc.get_service_nodes",
            [this, on_updated = std::move(on_updated)](bool success, std::vector<std::string> data) mutable {
                std::string fail_msg;
                if (not success)
                    fail_msg = "Failed to update service node list";
                else if (data.size() < 2)
                    fail_msg = "Oxend gave empty reply for service node list";
                else
                {
                    try
                    {
                        auto json = nlohmann::json::parse(std::move(data[1]));
                        if (json.at("status") != "OK")
                            throw std::runtime_error{"get_service_nodes did not return 'OK' status"};
                        if (auto it = json.find("unchanged"); it != json.end() and it->is_boolean() and it->get<bool>())
                            log::trace(logcat, "service node list unchanged");
                        else
                        {
                            handle_new_service_node_list(json.at("service_node_states"));
                            if (auto it = json.find("block_hash"); it != json.end() and it->is_string())
                                _last_hash_update = it->get<std::string>();
                            else
                                _last_hash_update.clear();
                            if (on_updated)
                                on_updated->set_value();
                        }
                    }
                    catch (const std::exception& ex)
                    {
                        fail_msg = fmt::format("Failed to process service node list: {}", ex.what());
                        log::error(logcat, "{}", fail_msg);
                    }
                }

                // set down here so that the 1) we don't start updating until we're completely
                // finished with the previous update; and 2) so that m_UpdatingList also guards
                // m_LastUpdateHash
                _is_updating_list = false;

                if (!fail_msg.empty() && on_updated)
                {
                    try
                    {
                        throw std::runtime_error{fail_msg};
                    }
                    catch (const std::runtime_error& e)
                    {
                        on_updated->set_exception(std::current_exception());
                    }
                }
            },
            req.dump());
    }

    void OxendRPC::ping()
    {
        // send a ping
        auto pk = _router.local_rid();

        nlohmann::json payload = {
            {"pubkey_ed25519", oxenc::to_hex(pk.begin(), pk.end())},
            {"version", {LOKINET_VERSION[0], LOKINET_VERSION[1], LOKINET_VERSION[2]}}};

        if (auto err = _router.OxendErrorState())
            payload["error"] = *err;

        request(
            "admin.lokinet_ping",
            [](bool success, std::vector<std::string> /* data */) {
                log::debug(logcat, "Received response for ping. Successful: {}", success);
            },
            payload.dump());

        // subscribe to block updates
        request("sub.block", [](bool success, std::vector<std::string> data) {
            if (data.empty() or not success)
                log::error(logcat, "Failed to subscribe to new blocks");
            else
                log::debug(logcat, "Subscribed to new blocks: {}", data[0]);
        });

        // Trigger an update on a regular timer as well in case we missed a block notify for
        // some reason (e.g. oxend restarts and loses the subscription); we poll using the last
        // known hash so that the poll is very cheap (basically empty) if the block hasn't
        // advanced.
        update_service_node_list();
    }

    void OxendRPC::start_pings()
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        log::info(logcat, "Starting OxendRPC ping ticker...");
        ping();
        _ping_ticker = _router.loop.call_every(PING_INTERVAL, [this] { ping(); });
    }

    void OxendRPC::handle_new_service_node_list(const nlohmann::json& j)
    {
        std::unordered_set<RouterID> registered;
        if (not j.is_array())
            throw std::runtime_error{"Invalid service node list: expected array of service node states"};

        for (auto& snode : j)
        {
            const auto ed_itr = snode.find("pubkey_ed25519");
            if (ed_itr == snode.end() or not ed_itr->is_string())
                continue;

            RouterID rid;
            if (rid.FromHex(ed_itr->get<std::string_view>()))
                registered.insert(rid);
        }

        if (registered.empty())
        {
            log::warning(logcat, "Ignoring empty/invalid service node list received from oxend");
            return;
        }

        // Thread-safe; doesn't need to be in a loop call:
        _router.node_db().set_registered_relays(std::move(registered));
    }

    void OxendRPC::inform_connection(RouterID router, bool success)
    {
        _router.loop.call([router, success, this]() {
            const nlohmann::json req = {{"passed", success}, {"pubkey", router.ToHex()}, {"type", "lokinet"}};
            request(
                "admin.report_peer_status",
                [](bool success, std::vector<std::string>) {
                    if (not success)
                    {
                        log::error(logcat, "Failed to report connection status to oxend");
                        return;
                    }
                    log::debug(logcat, "Reported connection status to core");
                },
                req.dump());
        });
    }

    Ed25519SecretKey OxendRPC::obtain_identity_key()
    {
        std::promise<Ed25519SecretKey> promise;
        request("admin.get_service_privkeys", [&promise](bool success, std::vector<std::string> data) {
            try
            {
                if (not success)
                    throw std::runtime_error("Failed to get private key request");

                if (data.empty() or data.size() < 2)
                    throw std::runtime_error("Failed to get private key request: data empty");

                const auto j = nlohmann::json::parse(data[1]);
                Ed25519SecretKey k;

                if (not k.FromHex(j.at("service_node_ed25519_privkey").get<std::string>()))
                    throw std::runtime_error("failed to parse private key");

                promise.set_value(k);
            }
            catch (const std::exception& e)
            {
                log::warning(logcat, "Caught exception while trying to request admin keys: {}", e.what());
                promise.set_exception(std::current_exception());
            }
            catch (...)
            {
                log::warning(logcat, "Caught non-standard exception while trying to request admin keys");
                promise.set_exception(std::current_exception());
            }
        });

        auto ftr = promise.get_future();
        return ftr.get();
    }

    void OxendRPC::lookup_sns_hash(
        std::string namehash, std::function<void(std::optional<EncryptedSNSRecord>)> resultHandler)
    {
        log::debug(logcat, "Looking Up ONS NameHash {}", namehash);
        const nlohmann::json req{{"type", 2}, {"name_hash", oxenc::to_hex(namehash)}};
        request(
            "rpc.sns_resolve",
            [this, resultHandler](bool success, std::vector<std::string> data) {
                std::optional<EncryptedSNSRecord> maybe = std::nullopt;
                if (success)
                {
                    try
                    {
                        EncryptedSNSRecord result;
                        const auto j = nlohmann::json::parse(data[1]);
                        j.dump();
                        result.ciphertext = oxenc::from_hex(j["encrypted_value"].get<std::string>());
                        const auto nonce = oxenc::from_hex(j["nonce"].get<std::string>());
                        if (nonce.size() != result.nonce.size())
                        {
                            throw std::invalid_argument{
                                fmt::format("nonce size mismatch: {} != {}", nonce.size(), result.nonce.size())};
                        }

                        std::copy_n(nonce.data(), nonce.size(), result.nonce.data());
                        maybe = result;
                    }
                    catch (std::exception& ex)
                    {
                        log::error(logcat, "Failed to parse response from ONS lookup: {}", ex.what());
                    }
                }
                _router.loop.call([resultHandler, maybe = std::move(maybe)]() { resultHandler(std::move(maybe)); });
            },
            req.dump());
    }

}  // namespace llarp::rpc
