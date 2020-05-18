#include <rpc/lokid_rpc_client.hpp>

#include <util/logging/logger.h>
#include <util/logging/logger.hpp>

#include <future>

namespace llarp
{
  namespace rpc
  {
    static ::LogLevel
    fromLokiMQLogLevel(lokimq::LogLevel level)
    {
      switch (level)
      {
        case lokimq::LogLevel::fatal:
        case lokimq::LogLevel::error:
          return eLogError;
        case lokimq::LogLevel::warn:
          return eLogWarn;
        case lokimq::LogLevel::info:
          return eLogInfo;
        case lokimq::LogLevel::debug:
        case lokimq::LogLevel::trace:
          return eLogDebug;
        default:
          return eLogNone;
      }
    }

    static lokimq::LogLevel
    toLokiMQLogLevel(::LogLevel level)
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

    static void
    lokimqLogger(lokimq::LogLevel level, const char* file, std::string msg)
    {
      switch (level)
      {
        case lokimq::LogLevel::fatal:
        case lokimq::LogLevel::error:
          LogError(msg);
          break;
        case lokimq::LogLevel::warn:
          LogWarn(msg);
          break;
        case lokimq::LogLevel::info:
          LogInfo(msg);
          break;
        case lokimq::LogLevel::debug:
        case lokimq::LogLevel::trace:
          LogDebug(msg);
          break;
      }
    }

    LokidRpcClient::LokidRpcClient(std::string lokidPubkey)
        : m_lokiMQ(lokimqLogger, lokimq::LogLevel::debug), m_lokidPubkey(std::move(lokidPubkey))
    {
      m_lokiMQ.log_level(toLokiMQLogLevel(LogLevel::Instance().curLevel));
    }

    void
    LokidRpcClient::connect()
    {
      m_lokidConnectionId = m_lokiMQ.connect_sn(m_lokidPubkey);  // not a blocking call
    }

    std::future<void>
    LokidRpcClient::ping()
    {
      throw std::runtime_error("TODO: LokidRpcClient::ping()");
    }

    std::future<std::string>
    LokidRpcClient::requestNextBlockHash()
    {
      throw std::runtime_error("TODO: LokidRpcClient::requestNextBlockHash()");
    }

    std::future<std::vector<RouterID>>
    LokidRpcClient::requestServiceNodeList()
    {
      throw std::runtime_error("TODO: LokidRpcClient::requestServiceNodeList()");
    }

    void
    LokidRpcClient::request()
    {
      // TODO: ensure we are connected
      // m_lokiMQ.request(m_lokidConnectionId, ...);

      throw std::runtime_error("TODO: LokidRpcClient::request()");
    }

  }  // namespace rpc
}  // namespace llarp
