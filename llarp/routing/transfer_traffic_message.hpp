#pragma once

#include <llarp/crypto/encrypted.hpp>
#include "message.hpp"
#include <llarp/service/protocol_type.hpp>

#include <vector>

namespace llarp
{
  namespace routing
  {
    constexpr size_t ExitPadSize = 512 - 48;
    constexpr size_t MaxExitMTU = 1500;
    constexpr size_t ExitOverhead = sizeof(uint64_t);
    struct TransferTrafficMessage final : public IMessage
    {
      std::vector<llarp::Encrypted<MaxExitMTU + ExitOverhead>> X;
      service::ProtocolType protocol;
      size_t _size = 0;

      void
      Clear() override
      {
        X.clear();
        _size = 0;
        version = 0;
        protocol = service::ProtocolType::TrafficV4;
      }

      size_t
      Size() const
      {
        return _size;
      }

      /// append buffer to X
      bool
      PutBuffer(const llarp_buffer_t& buf, uint64_t counter);

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* val) override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;
    };
  }  // namespace routing
}  // namespace llarp
