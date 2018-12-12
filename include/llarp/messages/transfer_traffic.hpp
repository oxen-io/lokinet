#ifndef LLARP_MESSAGES_TRANSFER_TRAFFIC_HPP
#define LLARP_MESSAGES_TRANSFER_TRAFFIC_HPP
#include <llarp/routing/message.hpp>
#include <llarp/encrypted.hpp>
#include <crypto.hpp>
#include <vector>

namespace llarp
{
  namespace routing
  {
    constexpr size_t ExitPadSize = 512 - 48;
    constexpr size_t MaxExitMTU  = 1500;
    struct TransferTrafficMessage final : public IMessage
    {
      std::vector< llarp::Encrypted > X;
      size_t _size = 0;

      size_t
      Size() const
      {
        return _size;
      }

      TransferTrafficMessage&
      operator=(const TransferTrafficMessage& other);

      /// append buffer to X
      bool
      PutBuffer(llarp_buffer_t buf, uint64_t counter);

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(llarp_buffer_t k, llarp_buffer_t* val) override;

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override;
    };
  }  // namespace routing
}  // namespace llarp

#endif
