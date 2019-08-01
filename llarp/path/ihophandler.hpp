#ifndef LLARP_PATH_IHOPHANDLER_HPP
#define LLARP_PATH_IHOPHANDLER_HPP

#include <crypto/types.hpp>
#include <util/types.hpp>
#include <crypto/encrypted_frame.hpp>

#include <memory>

struct llarp_buffer_t;

namespace llarp
{
  struct AbstractRouter;

  namespace routing
  {
    struct IMessage;
  }

  namespace path
  {
    struct IHopHandler
    {
      virtual ~IHopHandler()
      {
      }

      virtual bool
      Expired(llarp_time_t now) const = 0;

      virtual bool
      ExpiresSoon(llarp_time_t now, llarp_time_t dlt) const = 0;

      /// send routing message and increment sequence number
      virtual bool
      SendRoutingMessage(const routing::IMessage& msg, AbstractRouter* r) = 0;

      // handle data in upstream direction
      virtual bool
      HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                     AbstractRouter* r) = 0;

      // handle data in downstream direction
      virtual bool
      HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y,
                       AbstractRouter* r) = 0;

      /// return timestamp last remote activity happened at
      virtual llarp_time_t
      LastRemoteActivityAt() const = 0;

      virtual bool
      HandleLRSM(uint64_t status, std::array< EncryptedFrame, 8 >& frames,
                 AbstractRouter* r) = 0;

      uint64_t
      NextSeqNo()
      {
        return m_SequenceNum++;
      }

     protected:
      uint64_t m_SequenceNum = 0;
    };

    using HopHandler_ptr = std::shared_ptr< IHopHandler >;
  }  // namespace path
}  // namespace llarp
#endif
