#ifndef LLARP_SERVICE_TAG_HPP
#define LLARP_SERVICE_TAG_HPP

#include <llarp/aligned.hpp>

namespace llarp
{
  namespace service
  {
    struct Tag : public llarp::AlignedBuffer< 16 >
    {
      Tag() : llarp::AlignedBuffer< 16 >()
      {
        Zero();
      }

      Tag(const byte_t* d) : llarp::AlignedBuffer< 16 >(d)
      {
      }

      Tag(const std::string& str) : Tag()
      {
        memcpy(data(), str.c_str(), std::min(16UL, str.size()));
      }

      std::string
      ToString() const;
    };
  }  // namespace service
}  // namespace llarp

#endif