#ifndef LLARP_SERVICE_PROTOCOL_HPP
#define LLARP_SERVICE_PROTOCOL_HPP
#include <llarp/time.h>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <vector>

namespace llarp
{
  namespace service
  {
    enum ProtocolType
    {
      eProtocolText    = 0,
      eProtocolTraffic = 1
    };

    struct ProtocolMessage : public llarp::IBEncodeMessage
    {
      ProtocolMessage(ProtocolType t, uint64_t seqno);
      ~ProtocolMessage();
      ProtocolType proto;
      llarp_time_t queued = 0;
      std::vector< byte_t > payload;
      llarp::KeyExchangeNonce N;
      uint64_t sequenceNum;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);
      bool
      BEncode(llarp_buffer_t* buf) const;

      void
      PutBuffer(llarp_buffer_t payload);

      struct Compare
      {
        bool
        operator()(const ProtocolMessage* left,
                   const ProtocolMessage* right) const
        {
          return left->sequenceNum < right->sequenceNum;
        }
      };

      struct GetTime
      {
        llarp_time_t
        operator()(const ProtocolMessage* msg) const
        {
          return msg->queued;
        }
      };

      struct PutTime
      {
        void
        operator()(ProtocolMessage* msg, llarp_time_t now) const
        {
          msg->queued = now;
        }
      };
    };
  }  // namespace service
}  // namespace llarp

#endif