#ifndef TEST_UTIL_HPP
#define TEST_UTIL_HPP

#include <util/fs.hpp>

namespace llarp
{
  namespace test
  {
    std::string
    randFilename();

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
