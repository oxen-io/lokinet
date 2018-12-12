#ifndef LLARP_EXIT_POLICY_HPP
#define LLARP_EXIT_POLICY_HPP
#include <llarp/bencode.hpp>

namespace llarp
{
  namespace exit
  {
    struct Policy final : public llarp::IBEncodeMessage
    {
      ~Policy();

      uint64_t proto;
      uint64_t port;
      uint64_t drop;

      bool
      DecodeKey(llarp_buffer_t k, llarp_buffer_t* val) override;

      bool
      BEncode(llarp_buffer_t* buf) const override;
    };
  }  // namespace exit
}  // namespace llarp

#endif