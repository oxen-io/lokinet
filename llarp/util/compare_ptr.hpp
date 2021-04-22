#pragma once
#include <functional>

namespace llarp
{
  /// type for comparing smart pointer's managed values
  template <typename Ptr_t, typename Compare = std::less<>>
  struct ComparePtr
  {
    bool
    operator()(const Ptr_t& left, const Ptr_t& right) const
    {
      if (left && right)
        return Compare()(*left, *right);

      return Compare()(left, right);
    }
  };
}  // namespace llarp
