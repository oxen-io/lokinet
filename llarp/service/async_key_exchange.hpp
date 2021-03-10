#pragma once

#include <llarp/crypto/types.hpp>
#include "identity.hpp"
#include "protocol.hpp"

namespace llarp
{
  namespace service
  {
    struct AsyncKeyExchange : public std::enable_shared_from_this<AsyncKeyExchange>
    {
      EventLoop_ptr loop;
      SharedSecret sharedKey;
      ServiceInfo m_remote;
      const Identity& m_LocalIdentity;
      ProtocolMessage msg;
      Introduction intro;
      const PQPubKey introPubKey;
      Introduction remoteIntro;
      std::function<void(std::shared_ptr<ProtocolFrame>)> hook;
      IDataHandler* handler;
      ConvoTag tag;

      AsyncKeyExchange(
          EventLoop_ptr l,
          ServiceInfo r,
          const Identity& localident,
          const PQPubKey& introsetPubKey,
          const Introduction& remote,
          IDataHandler* h,
          const ConvoTag& t,
          ProtocolType proto);

      static void
      Result(std::shared_ptr<AsyncKeyExchange> user, std::shared_ptr<ProtocolFrame> frame);

      /// given protocol message make protocol frame
      static void
      Encrypt(std::shared_ptr<AsyncKeyExchange> user, std::shared_ptr<ProtocolFrame> frame);
    };

  }  // namespace service
}  // namespace llarp
