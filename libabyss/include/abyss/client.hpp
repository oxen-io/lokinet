#ifndef __ABYSS_CLIENT_HPP__
#define __ABYSS_CLIENT_HPP__
#include <string>
#include <memory>
#include <list>
#include <deque>
#include <unordered_map>
#include <functional>
#include <llarp/string_view.hpp>
#include <abyss/json.hpp>
#include <llarp/ev.h>

namespace abyss
{
  namespace http
  {
    typedef std::string RPC_Method_t;
    typedef json::Value RPC_Params;
    typedef json::Document RPC_Response;
    typedef std::unordered_multimap< std::string, std::string > Headers_t;
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
      HandleResponse(const RPC_Response& response) = 0;

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
      typedef std::function< IRPCClientHandler*(ConnImpl*) > HandlerFactory;

      JSONRPC();
      ~JSONRPC();

      /// start runing on event loop async
      /// return true on success otherwise return false
      bool
      RunAsync(llarp_ev_loop* loop, const std::string& endpoint);

      /// must be called after RunAsync returns true
      /// queue a call for rpc
      void
      QueueRPC(RPC_Method_t method, RPC_Params params,
               HandlerFactory createHandler);

      /// drop all pending calls on the floor
      void
      DropAllCalls();

      /// handle new outbound connection
      void
      Connected(llarp_tcp_conn* conn);

      /// flush queued rpc calls
      void
      Flush();

     private:
      struct Call
      {
        Call(RPC_Method_t&& m, RPC_Params&& p, HandlerFactory&& f)
            : method(std::move(m))
            , params(std::move(p))
            , createHandler(std::move(f))
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

      llarp_tcp_connecter m_connect;
      llarp_ev_loop* m_Loop;
      std::deque< Call > m_PendingCalls;
      std::list< std::unique_ptr< IRPCClientHandler > > m_Conns;
    };

  }  // namespace http
}  // namespace abyss

#endif