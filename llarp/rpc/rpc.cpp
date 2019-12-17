#include <rpc/rpc.hpp>

#include <zmq.hpp>

#include <constants/version.hpp>
#include <router/abstractrouter.hpp>
#include <service/context.hpp>
#include <util/logging/logger.hpp>
#include <router_id.hpp>
#include <exit/context.hpp>
#include <rpc/json_rpc_dispatcher.hpp>

#include <util/encode.hpp>
#include <util/meta/memfn.hpp>
#include <utility>

namespace llarp
{
  namespace rpc
  {
    struct ServerImpl
    {
      AbstractRouter* m_router;
      JsonRpcDispatcher m_rpcDispatcher;

      zmq::context_t m_context;
      std::unique_ptr< zmq::socket_t > m_socket;
      zmq::message_t m_recvBuffer;

      /**
       * Constructor
       */
      ServerImpl(AbstractRouter* r) : m_router(r), m_context()
      {
        m_rpcDispatcher.setHandler("ping", [](const nlohmann::json& params) {
          (void)params;
          return response_t{false, "pong"};
        });
      }

      /**
       * Listen on the specified port.
       */
      bool
      start()
      {
        if(m_socket)
          return false;

        try
        {
          m_socket.reset(new zmq::socket_t(m_context, ZMQ_REP));
          m_socket->bind("ipc:///tmp/lokinetrpc");
        }
        catch(const std::exception& e)
        {
          LogWarn("Caught exception while trying to bring up zmq", e.what());
          m_socket.release();
          return false;
        }

        return true;
      }

      /**
       * Stop listening
       */
      bool
      stop()
      {
        if(!m_socket)
          return false;

        m_socket.reset(nullptr);
        return true;
      }

      /**
       * Tick - handle any incoming requests
       */
      void
      tick(llarp_time_t now)
      {
        (void)now;  // unused

        try
        {
          auto result = m_socket->recv(m_recvBuffer, zmq::recv_flags::dontwait);
          if(result)
          {
            // TODO: avoid copy ?
            std::string request((const char*)m_recvBuffer.data(),
                                m_recvBuffer.size());

            nlohmann::json responseJson = m_rpcDispatcher.processRaw(request);
            std::string responseStr     = responseJson.dump();

            zmq::message_t response(responseStr.c_str(), responseStr.size());
            m_socket->send(std::move(response), zmq::send_flags::dontwait);
          }
        }
        catch(const std::exception& e)
        {
          LogWarn("Caught exception while trying to recv() on ZMQ socket",
                  e.what());
          LogWarn(
              "ZMQ requires explicit request/response pairing, we may be in a "
              "bad state");
          // TODO: as this warning suggests, we need to make sure we always send
          // a response if
          //       we received a request
        }
      }
    };

    Server::Server(AbstractRouter* r) : m_Impl(new ServerImpl(r))
    {
    }

    Server::~Server() = default;

    bool
    Server::Stop()
    {
      return m_Impl->stop();
    }

    bool
    Server::Start()
    {
      return m_Impl->start();
    }

    void
    Server::Tick(llarp_time_t now)
    {
      m_Impl->tick(now);
    }

  }  // namespace rpc
}  // namespace llarp
