#ifndef LLARP_MESSAGES_PATH_TRANSFER_HPP
#define LLARP_MESSAGES_PATH_TRANSFER_HPP

#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <routing/message.hpp>
#include <service/protocol.hpp>

namespace llarp
{
  namespace routing
  {
    struct PathTransferMessage final : public IMessage
    {
      PathID_t P;
      service::ProtocolFrame T;
      TunnelNonce Y;

      PathTransferMessage() = default;
      PathTransferMessage(const service::ProtocolFrame& f, const PathID_t& p)
          : P(p), T(f)
      {
        Y.Randomize();
      }
      ~PathTransferMessage() override = default;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      HandleMessage(IMessageHandler*, AbstractRouter* r) const override;

      void
      Clear() override
      {
        P.Zero();
        T.Clear();
        Y.Zero();
        version = 0;
      }
    };

  }  // namespace routing
}  // namespace llarp

#endif
