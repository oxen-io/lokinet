#ifndef LLARP_UTIL_COMPARE_PTR_HPP
#define LLARP_UTIL_COMPARE_PTR_HPP
#include <functional>

namespace llarp
{
  /// type for comparing smart pointer's managed values
  template < typename Ptr_t,
             typename Compare = std::less< typename Ptr_t::element_type > >
  struct ComparePtr
  {
    bool
    operator()(const Ptr_t& left, const Ptr_t& right) const
    {
      if(left && right)
        return Compare()(*left, *right);
      return false;
    }
  };
}  // namespace llarp

#endif