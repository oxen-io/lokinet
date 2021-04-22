#pragma once

#include <llarp/crypto/constants.hpp>
#include <llarp/util/aligned.hpp>

namespace llarp
{
  struct PathID_t final : public AlignedBuffer<PATHIDSIZE>
  {
    using AlignedBuffer<PATHIDSIZE>::AlignedBuffer;
  };

}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::PathID_t> : hash<llarp::AlignedBuffer<llarp::PathID_t::SIZE>>
  {};
}  // namespace std
