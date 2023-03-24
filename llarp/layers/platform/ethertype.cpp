#include "ethertype.hpp"
#include "llarp/util/time.hpp"

namespace llarp::layers::platform
{
  std::string
  ToString(EtherType_t kind)
  {
    const std::unordered_map<EtherType_t, std::string> loopkup{
        {EtherType_t::ip_unicast, "ip_unicast"},
        {EtherType_t::plainquic, "plainquic"},
        {EtherType_t::proto_auth, "auth"}};
    return loopkup.at(kind);
  }
}  // namespace llarp::layers::platform
