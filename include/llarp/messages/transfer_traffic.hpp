#ifndef LLARP_MESSAGES_TRANSFER_TRAFFIC_HPP
#define LLARP_MESSAGES_TRANSFER_TRAFFIC_HPP
#include <llarp/routing/message.hpp>
#include <llarp/crypto.hpp>
#include <vector>

namespace llarp
{
  namespace routing
  {
    constexpr size_t MaxExitMTU = 1500;
    struct TransferTrafficMessage final : public IMessage
    {
      std::vector< byte_t > X;

      TransferTrafficMessage&
      operator=(const TransferTrafficMessage& other);

      bool
      PutBuffer(llarp_buffer_t buf);

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t k, llarp_buffer_t* val) override;

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const override;
    };
  }  // namespace routing
}  // namespace llarp

#endif