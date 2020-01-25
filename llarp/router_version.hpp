#ifndef LLARP_ROUTER_VERSION_HPP
#define LLARP_ROUTER_VERSION_HPP

#include <array>
#include <util/bencode.hpp>

namespace llarp
{
  struct RouterVersion : public std::array< uint64_t, 4 >
  {
    RouterVersion() = default;

    explicit RouterVersion(const std::array< uint64_t, 4 >&);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);

    /// return true if this router version is all zeros
    bool
    IsEmpty() const;

    /// set to be empty
    void
    Clear();

    std::string
    ToString() const;
  };

  inline std::ostream&
  operator<<(std::ostream& out, const RouterVersion& rv)
  {
    return out << rv.ToString();
  }
}  // namespace llarp

#endif
