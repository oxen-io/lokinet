#include <dns/name.hpp>
#include <dns/rr.hpp>
#include <dns/serialize.hpp>
#include <dns/message.hpp>
#include <dns/server.hpp>

namespace llarp
{
  namespace dns
  {
    constexpr uint16_t qTypeTXT   = 16;
    constexpr uint16_t qTypeMX    = 15;
    constexpr uint16_t qTypePTR   = 12;
    constexpr uint16_t qTypeCNAME = 5;
    constexpr uint16_t qTypeNS    = 2;
    constexpr uint16_t qTypeA     = 1;

    constexpr uint16_t qClassIN = 1;
  }  // namespace dns
}  // namespace llarp