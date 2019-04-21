#ifndef LLARP_SERVICE_ASYNC_KEY_EXCHANGE_HPP
#define LLARP_SERVICE_ASYNC_KEY_EXCHANGE_HPP

#include <crypto/types.hpp>
#include <service/Identity.hpp>
#include <service/protocol.hpp>

namespace llarp
{
  class Logic;
  struct Crypto;

  namespace service
  {
    struct AsyncKeyExchange
    {
      Logic* logic;
      Crypto* crypto;
      SharedSecret sharedKey;
      ServiceInfo remote;
      const Identity& m_LocalIdentity;
      ProtocolMessage msg;
      ProtocolFrame frame;
      Introduction intro;
      const PQPubKey introPubKey;
      Introduction remoteIntro;
      std::function< void(ProtocolFrame&) > hook;
      IDataHandler* handler;
      ConvoTag tag;

      AsyncKeyExchange(Logic* l, Crypto* c, const ServiceInfo& r,
                       const Identity& localident,
                       const PQPubKey& introsetPubKey,
                       const Introduction& remote, IDataHandler* h,
                       const ConvoTag& t);

      static void
      Result(void* user);

      /// given protocol message make protocol frame
      static void
      Encrypt(void* user);
    };

  }  // namespace service
}  // namespace llarp

#endif
