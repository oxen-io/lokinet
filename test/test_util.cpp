#include <test_util.hpp>

#include <random>

namespace llarp
{
  namespace test
  {
    std::string
    randFilename()
    {
      static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";

      std::random_device rd;
      std::uniform_int_distribution< size_t > dist{0, sizeof(alphabet) - 2};

      std::string filename;
      for(size_t i = 0; i < 5; ++i)
      {
        filename.push_back(alphabet[dist(rd)]);
      }

      filename.push_back('.');

      for(size_t i = 0; i < 5; ++i)
      {
        filename.push_back(alphabet[dist(rd)]);
      }

      return filename;
    }
  }  // namespace test
}  // namespace llarp
