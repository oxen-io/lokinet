#ifndef LLARP_SERVICE_TAG_HPP
#define LLARP_SERVICE_TAG_HPP

#include <aligned.hpp>
#include <dht/key.hpp>

#include <sodium/crypto_generichash.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

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
        // evidently, does nothing on LP64 systems (where size_t is *already*
        // unsigned long but zero-extends this on LLP64 systems
        memcpy(data(), str.c_str(), std::min(16UL, (unsigned long)str.size()));
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
        memcpy(data(), str.data(), std::min(16UL, (unsigned long)str.size()));
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
