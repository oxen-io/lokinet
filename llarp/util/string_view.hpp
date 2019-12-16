#ifndef LLARP_STRING_VIEW_HPP
#define LLARP_STRING_VIEW_HPP

#include <absl/hash/hash.h>
#include <absl/strings/string_view.h>
namespace llarp
{
  using string_view      = absl::string_view;
  using string_view_hash = absl::Hash< string_view >;

  static std::string
  string_view_string(const string_view& v)
  {
    return std::string(v);
  }
}  // namespace llarp
#endif
