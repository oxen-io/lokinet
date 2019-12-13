#include <constants/limits.hpp>
#include <cctype>
#include <util/buffer.hpp>

namespace llarp
{
  namespace limits
  {
    /// snode limit parameters
    const LimitParameters snode = {6, 60};

    /// client limit parameters
    const LimitParameters client = {4, 6};

    const LNSLimits lns = {248};

    bool
    LNSLimits::NameIsValid(const llarp_buffer_t& buf) const
    {
      static constexpr size_t LokiAddrSize = 52;
      if(buf.sz > MaxNameSize || buf.sz == LokiAddrSize)
        return false;
      byte_t* ptr = buf.base;
      for(size_t idx = 0; idx < buf.sz; ++idx)
      {
        if(std::isalnum(ptr[idx]))
          continue;
        if(ptr[idx] == '_')
          continue;
        if(ptr[idx] == '-')
          continue;
        return false;
      }
      return true;
    }
  }  // namespace limits
}  // namespace llarp
