#include <constants/limits.hpp>

namespace llarp
{
  namespace limits
  {
    /// snode limit parameters
    const LimitParameters snode = {6, 60};

    /// client limit parameters
    const LimitParameters client = {4, 6};
  }  // namespace limits
}  // namespace llarp