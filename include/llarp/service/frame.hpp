#ifndef LLARP_SERVICE_FRAME_HPP
#define LLARP_SERVICE_FRAME_HPP
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/encrypted.hpp>

namespace llarp
{
  namespace service
  {
    struct DataFrame : public llarp::IBEncodeMessage
    {
      llarp::Encrypted D;
      llarp::PubKey H;
      llarp::KeyExchangeNonce N;
      uint64_t S = 0;
      llarp::Signature Z;

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);
    };
  }  // namespace service
}  // namespace llarp

#endif