#ifndef TEST_UTIL_HPP
#define TEST_UTIL_HPP

#include <util/fs.hpp>
#include <util/types.hpp>

namespace llarp
{
  namespace test
  {
    std::string
    randFilename();

    template < typename Buf >
    Buf
    makeBuf(byte_t val)
    {
      Buf b;
      b.Fill(val);
      return b;
    }

    struct FileGuard
    {
      const fs::path &p;

      explicit FileGuard(const fs::path &_p) : p(_p)
      {
      }

      ~FileGuard()
      {
        if(fs::exists(fs::status(p)))
        {
          fs::remove(p);
        }
      }
    };

  }  // namespace test
}  // namespace llarp

#endif
