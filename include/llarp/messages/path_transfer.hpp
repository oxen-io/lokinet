#ifndef LLARP_MESSAGES_PATH_TRANSFER_HPP
#define LLARP_MESSAGES_PATH_TRANSFER_HPP

#include <llarp/crypto.hpp>
#include <llarp/encrypted.hpp>
#include <llarp/routing/message.hpp>

namespace llarp
{
  namespace routing
  {
    struct PathTransferMessage : public IMessage
    {
      PathID_t P;
      Encrypted T;
      TunnelNonce Y;
    };

  }  // namespace routing
}  // namespace llarp

#endif