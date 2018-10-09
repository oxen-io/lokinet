#ifndef LLARP_ARPC_HPP
#define LLARP_ARPC_HPP

#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/logger.hpp>
#include <llarp/time.h>
#include <llarp/endian.h>
#include <llarp/ev.h>

#include <functional>
#include <string>
#include <map>
#include <unordered_map>

#ifndef _WIN32
#include <sys/un.h>
#endif

#include <llarp/net.hpp>

// forward declare
struct llarp_router;

namespace llarp
{
  namespace arpc
  {
    // forward declare
    struct BaseMessage;

    struct Server
    {
      llarp_tcp_acceptor m_acceptor;

      llarp_router* router;
      Server(llarp_router* r);

      static void
      OnAccept(llarp_tcp_acceptor* a, llarp_tcp_conn* conn);

      bool
      Start(const std::string& bindaddr);

      const llarp_crypto*
      Crypto() const;

      const byte_t*
      SigningPublicKey() const
      {
        return llarp::seckey_topublic(SigningPrivateKey());
      }

      const byte_t*
      SigningPrivateKey() const;

      bool
      Sign(BaseMessage* msg) const;
    };

  }  // namespace arpc
}  // namespace llarp

#endif
