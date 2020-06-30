#include <rpc/lokid_rpc_client.hpp>

#include <util/logging/logger.hpp>

#include <router/abstractrouter.hpp>

#include <nlohmann/json.hpp>

#include <util/time.hpp>
#include <util/thread/logic.hpp>

namespace llarp
{
  namespace rpc
  {
    static lokimq::LogLevel
    toLokiMQLogLevel(llarp::LogLevel level)
    {
      switch (level)
      {
        case eLogError:
          return lokimq::LogLevel::error;
        case eLogWarn:
          return lokimq::LogLevel::warn;
        case eLogInfo:
          return lokimq::LogLevel::info;
        case eLogDebug:
          return lokimq::LogLevel::debug;
        case eLogNone:
        default:
          return lokimq::LogLevel::trace;
      }
    }

    LokidRpcClient::LokidRpcClient(LMQ_ptr lmq, AbstractRouter* r)
        : m_lokiMQ(std::move(lmq)), m_Router(r)
    {
      // m_lokiMQ->log_level(toLokiMQLogLevel(LogLevel::Instance().curLevel));
    }

    void
    LokidRpcClient::ConnectAsync(lokimq::address url)
    {
      LogInfo("connecting to lokid via LMQ at ", url);
      m_lokiMQ->connect_remote(
          url.zmq_address(),
          [self = shared_from_this()](lokimq::ConnectionID c) {
            self->m_Connection = std::move(c);
            self->Connected();
          },
          [self = shared_from_this(), url](lokimq::ConnectionID, std::string_view f) {
            llarp::LogWarn("Failed to connect to lokid: ", f);
            LogicCall(self->m_Router->logic(), [self, url]() { self->ConnectAsync(url); });
          });
    }

    void
    LokidRpcClient::Command(std::string_view cmd)
    {
      LogDebug("lokid command: ", cmd);
      m_lokiMQ->send(*m_Connection, std::move(cmd));
    }

    void
    LokidRpcClient::UpdateServiceNodeList()
    {
      nlohmann::json request;
      request["pubkey_ed25519"] = true;
      request["active_only"] = true;
      if (not m_CurrentBlockHash.empty())
        request["poll_block_hash"] = m_CurrentBlockHash;
      Request(
          "rpc.get_service_nodes",
          [self = shared_from_this()](bool success, std::vector<std::string> data) {
            if (not success)
            {
              LogWarn("failed to update service node list");
              return;
            }
            if (data.size() < 2)
            {
              LogWarn("lokid gave empty reply for service node list");
              return;
            }
            try
            {
              self->HandleGotServiceNodeList(std::move(data[1]));
            }
            catch (std::exception& ex)
            {
              LogError("failed to process service node list: ", ex.what());
            }
          },
          request.dump());
    }

    void
    LokidRpcClient::Connected()
    {
      constexpr auto PingInterval = 1min;
      constexpr auto NodeListUpdateInterval = 30s;

      LogInfo("we connected to lokid [", *m_Connection, "]");
      Command("admin.lokinet_ping");
      m_lokiMQ->add_timer(
          [self = shared_from_this()]() { self->Command("admin.lokinet_ping"); }, PingInterval);
      m_lokiMQ->add_timer(
          [self = shared_from_this()]() { self->UpdateServiceNodeList(); }, NodeListUpdateInterval);
      UpdateServiceNodeList();
    }

    void
    LokidRpcClient::HandleGotServiceNodeList(std::string data)
    {
      auto j = nlohmann::json::parse(std::move(data));
      {
        const auto itr = j.find("block_hash");
        if (itr != j.end())
        {
          m_CurrentBlockHash = itr->get<std::string>();
        }
      }
      {
        const auto itr = j.find("unchanged");
        if (itr != j.end())
        {
          if (itr->get<bool>())
          {
            LogDebug("service node list unchanged");
            return;
          }
        }
      }

      std::vector<RouterID> nodeList;
      {
        const auto itr = j.find("service_node_states");
        if (itr != j.end() and itr->is_array())
        {
          for (auto j_itr = itr->begin(); j_itr != itr->end(); j_itr++)
          {
            const auto ed_itr = j_itr->find("pubkey_ed25519");
            if (ed_itr == j_itr->end() or not ed_itr->is_string())
              continue;
            RouterID rid;
            if (rid.FromHex(ed_itr->get<std::string>()))
              nodeList.emplace_back(std::move(rid));
          }
        }
      }

      if (nodeList.empty())
      {
        LogWarn("got empty service node list from lokid");
        return;
      }
      // inform router about the new list
      LogicCall(m_Router->logic(), [r = m_Router, nodeList]() { r->SetRouterWhitelist(nodeList); });
    }

    SecretKey
    LokidRpcClient::ObtainIdentityKey()
    {
      std::promise<SecretKey> promise;
      Request(
          "admin.get_service_privkeys",
          [self = shared_from_this(), &promise](bool success, std::vector<std::string> data) {
            try
            {
              if (not success)
              {
                throw std::runtime_error(
                    "failed to get private key request "
                    "failed");
              }
              if (data.empty())
              {
                throw std::runtime_error(
                    "failed to get private key request "
                    "data empty");
              }
              const auto j = nlohmann::json::parse(data[0]);
              SecretKey k;
              if (not k.FromHex(j.at("service_node_ed25519_privkey").get<std::string>()))
              {
                throw std::runtime_error("failed to parse private key");
              }
              promise.set_value(k);
            }
            catch (...)
            {
              promise.set_exception(std::current_exception());
            }
          });
      auto ftr = promise.get_future();
      return ftr.get();
    }

  }  // namespace rpc
}  // namespace llarp
