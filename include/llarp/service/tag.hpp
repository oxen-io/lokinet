#ifndef LLARP_SERVICE_TAG_HPP
#define LLARP_SERVICE_TAG_HPP

#include <sodium/crypto_generichash.h>
#include <llarp/aligned.hpp>
#include <llarp/dht/key.hpp>

namespace llarp
{
  namespace service
  {
    struct Tag : public llarp::AlignedBuffer< 16 >
    {
      Tag() : llarp::AlignedBuffer< 16 >()
      {
      }

      Tag(const byte_t* d) : llarp::AlignedBuffer< 16 >(d)
      {
      }

      Tag(const std::string& str) : Tag()
      {
#ifndef MIN
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
        memcpy(data(), str.c_str(), MIN(16UL, str.size()));
#undef MIN
#endif
      }

      Tag&
      operator=(const Tag& other)
      {
        memcpy(data(), other.data(), 16);
        return *this;
      }

      Tag&
      operator=(const std::string& str)
      {
#ifndef MIN
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
        memcpy(data(), str.data(), MIN(16UL, str.size()));
#undef MIN
#endif
        return *this;
      }

      std::string
      ToString() const;

      bool
      Empty() const
      {
        return ToString().empty();
      }

      struct Hash
      {
        std::size_t
        operator()(const Tag& t) const
        {
          return *t.data_l();
        }
      };
    };
  }  // namespace service
}  // namespace llarp

#endif