#ifndef LLARP_MESSAGES_PATH_TRANSFER_HPP
#define LLARP_MESSAGES_PATH_TRANSFER_HPP

#include <llarp/crypto.hpp>
#include <llarp/encrypted.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/service/protocol.hpp>

namespace llarp
{
  namespace routing
  {
    struct PathTransferMessage : public IMessage
    {
      PathID_t P;
      service::ProtocolFrame T;
      TunnelNonce Y;

      PathTransferMessage();
      PathTransferMessage(const service::ProtocolFrame& f, const PathID_t& p)
          : P(p), T(f)
      {
        Y.Randomize();
      }
      ~PathTransferMessage();

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      HandleMessage(IMessageHandler*, llarp_router* r) const;
    };

  }  // namespace routing
}  // namespace llarp

#endif
