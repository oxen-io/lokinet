#ifndef LLARP_UTP_INBOUND_MESSAGE_HPP
#define LLARP_UTP_INBOUND_MESSAGE_HPP

#include <constants/link_layer.hpp>
#include <util/aligned.hpp>
#include <util/types.hpp>

#include <utp_types.h>  // for uint32

#include <string.h>

#include <util/alloc.hpp>

namespace llarp
{
  namespace utp
  {
    /// size of keyed hash
    constexpr size_t FragmentHashSize = 32;
    /// size of outer nonce
    constexpr size_t FragmentNonceSize = 32;
    /// size of outer overhead
    constexpr size_t FragmentOverheadSize =
        FragmentHashSize + FragmentNonceSize;
    /// max fragment payload size
    constexpr size_t FragmentBodyPayloadSize = 512;
    /// size of inner nonce
    constexpr size_t FragmentBodyNonceSize = 24;
    /// size of fragment body overhead
    constexpr size_t FragmentBodyOverhead = FragmentBodyNonceSize
        + sizeof(uint32) + sizeof(uint16_t) + sizeof(uint16_t);
    /// size of fragment body
    constexpr size_t FragmentBodySize =
        FragmentBodyOverhead + FragmentBodyPayloadSize;
    /// size of fragment
    constexpr size_t FragmentBufferSize =
        FragmentOverheadSize + FragmentBodySize;

    static_assert(FragmentBufferSize == 608, "Fragment Buffer Size is not 608");

    /// buffer for a single utp fragment
    using FragmentBuffer = AlignedBuffer< FragmentBufferSize >;

    /// maximum size for send queue for a session before we drop
    constexpr size_t MaxSendQueueSize = 64;

    /// buffer for a link layer message
    using MessageBuffer = AlignedBuffer< MAX_LINK_MSG_SIZE >;

    /// pending inbound message being received
    struct _InboundMessage
    {
      /// timestamp of last activity
      llarp_time_t lastActive;
      /// the underlying message buffer
      MessageBuffer _msg;

      /// for accessing message buffer
      llarp_buffer_t buffer;

      /// return true if this inbound message can be removed due to expiration
      bool
      IsExpired(llarp_time_t now) const;

      /// append data at ptr of size sz bytes to message buffer
      /// increment current position
      /// return false if we don't have enough room
      /// return true on success
      bool
      AppendData(const byte_t* ptr, uint16_t sz);

      _InboundMessage() : lastActive(0), _msg(), buffer(_msg)
      {
      }
    };

    inline bool
    operator==(const _InboundMessage& lhs, const _InboundMessage& rhs)
    {
      return lhs.buffer.base == rhs.buffer.base;
    }

    using InboundMessage = std::shared_ptr< _InboundMessage >;

  }  // namespace utp

}  // namespace llarp

#endif
