#ifndef ABYSS_CLIENT_HPP
#define ABYSS_CLIENT_HPP

#include <ev/ev.h>
#include <util/json.hpp>
#include <util/string_view.hpp>
#include <abyss/http.hpp>

#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>

namespace abyss
{
  namespace http
  {
    using RPC_Method_t = std::string;
    using RPC_Params = nlohmann::json;
    using RPC_Response = nlohmann::json;
    using Headers_t = std::unordered_multimap<std::string, std::string>;
    using Response = RequestHeader;
    struct ConnImpl;

    /// jsonrpc response handler for client
    struct IRPCClientHandler
    {
      IRPCClientHandler(ConnImpl* impl);
      virtual ~IRPCClientHandler();

      /// handle response from rpc server
      /// return true on successful handling
      /// return false on errors while handling
      virtual bool
      HandleResponse(RPC_Response response) = 0;

      /// populate http request headers
      virtual void
      PopulateReqHeaders(Headers_t& hdr) = 0;

      /// handle fatal internal error while doing request
      virtual void
      HandleError() = 0;

      /// return true if we should close
      bool
      ShouldClose() const;

      /// close underlying connection
      void
      Close() const;

     private:
      ConnImpl* m_Impl;
    };

    /// jsonrpc client
    struct JSONRPC
    {
      using HandlerFactory = std::function<IRPCClientHandler*(ConnImpl*)>;

      JSONRPC();
      ~JSONRPC();

      /// start runing on event loop async
      /// return true on success otherwise return false
      bool
      RunAsync(llarp_ev_loop_ptr loop, const std::string& endpoint);

      /// must be called after RunAsync returns true
      /// queue a call for rpc
      void
      QueueRPC(RPC_Method_t method, RPC_Params params, HandlerFactory createHandler);

      /// drop all pending calls on the floor
      void
      DropAllCalls();

      /// close all connections and stop operation
      void
      Stop();

      /// handle new outbound connection
      void
      Connected(llarp_tcp_conn* conn);

      /// flush queued rpc calls
      void
      Flush();

      std::string username;
      std::string password;

     private:
      struct Call
      {
        Call(RPC_Method_t&& m, RPC_Params&& p, HandlerFactory&& f)
            : method(std::move(m)), params(std::move(p)), createHandler(std::move(f))
        {
        }
        RPC_Method_t method;
        RPC_Params params;
        HandlerFactory createHandler;
      };

      static void
      OnConnected(llarp_tcp_connecter* connect, llarp_tcp_conn* conn);

      static void
      OnConnectFail(llarp_tcp_connecter* connect);

      static void
      OnTick(llarp_tcp_connecter* connect);

      std::atomic<bool> m_Run;
      llarp_tcp_connecter m_connect;
      llarp_ev_loop_ptr m_Loop;
      std::deque<Call> m_PendingCalls;
      std::list<std::unique_ptr<IRPCClientHandler>> m_Conns;
    };
  }  // namespace http
}  // namespace abyss

#endif
