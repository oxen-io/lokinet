#ifndef TEST_UTIL_HPP
#define TEST_UTIL_HPP

#include <util/fs.hpp>
#include <util/types.hpp>

#include <bitset>
#include <vector>

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
      const fs::path p;

      FileGuard(const fs::path &_p) : p(_p)
      {
      }

      ~FileGuard()
      {
        if(fs::exists(fs::status(p)))
        {
          fs::remove_all(p);
        }
      }
    };

    inline void
    randbytes_impl(byte_t *ptr, size_t sz)
    {
      std::fill_n(ptr, sz, 0xAA);
    }

    template < typename T >
    inline void
    keygen_val(T &val, byte_t x)
    {
      val.Fill(x);
    }

    template < typename T >
    inline void
    keygen(T &val)
    {
      keygen_val(val, 0xAA);
    }

    template < typename T >
    struct CombinationIterator
    {
      std::vector< T > toCombine;
      std::vector< T > currentCombo;

      int bits;
      int maxBits;

      void
      createCombo()
      {
        currentCombo.clear();
        for(size_t i = 0; i < toCombine.size(); ++i)
        {
          if(bits & (1 << i))
          {
            currentCombo.push_back(toCombine[i]);
          }
        }
      }

      CombinationIterator(const std::vector< T > &values)
          : toCombine(values), bits(0), maxBits((1 << values.size()) - 1)
      {
        currentCombo.reserve(values.size());
        createCombo();
      }

      bool
      next()
      {
        if(bits >= maxBits)
        {
          return false;
        }

        ++bits;
        createCombo();
        return true;
      }

      bool
      includesElement(size_t index)
      {
        return bits & (1 << index);
      }
    };

  }  // namespace test
}  // namespace llarp

#endif
