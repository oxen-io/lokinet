#ifndef LLARP_SERVICE_TAG_HPP
#define LLARP_SERVICE_TAG_HPP

#include <dht/key.hpp>
#include <util/aligned.hpp>
#include <util/status.hpp>

#include <sodium/crypto_generichash.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

namespace llarp
{
  namespace service
  {
    struct Tag : public AlignedBuffer<16>
    {
      Tag() : AlignedBuffer<SIZE>()
      {
      }

      Tag(const byte_t* d) : AlignedBuffer<SIZE>(d)
      {
      }

      Tag(const std::string& str) : Tag()
      {
        // evidently, does nothing on LP64 systems (where size_t is *already*
        // unsigned long but zero-extends this on LLP64 systems
        // 2Jan19: reeee someone undid the patch
        std::copy(
            str.begin(), str.begin() + std::min(std::string::size_type(16), str.size()), begin());
      }

      Tag&
      operator=(const std::string& str)
      {
        std::copy(
            str.begin(), str.begin() + std::min(std::string::size_type(16), str.size()), begin());
        return *this;
      }

      util::StatusObject
      ExtractStatus() const
      {
        return util::StatusObject{{"name", ToString()}};
      }

      std::string
      ToString() const;

      bool
      Empty() const
      {
        return data()[0] == 0;
      }

      using Hash = AlignedBuffer<SIZE>::Hash;
    };
  }  // namespace service
}  // namespace llarp

#endif
