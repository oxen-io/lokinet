#include "client_contact.hpp"

#include <llarp/util/logging.hpp>

namespace llarp
{
    static auto logcat = log::Cat("client-intro");

    ClientIntro::ClientIntro(oxenc::bt_dict_consumer&& btdc)
    {
        expiry = std::chrono::sys_seconds{std::chrono::seconds{btdc.require<int64_t>("e")}};
        hop.assign(btdc.require_span<std::byte, HopID::SIZE>("h"));
        relay.assign(btdc.require_span<std::byte, RouterID::SIZE>("r"));
    }

    ClientIntro::ClientIntro(std::string_view buf) : ClientIntro{oxenc::bt_dict_consumer{buf}} {}

    void ClientIntro::bt_encode(oxenc::bt_dict_producer&& subdict) const
    {
        subdict.append("e", expiry.time_since_epoch().count());
        subdict.append("h", hop.to_view());
        subdict.append("r", relay.to_view());
    }

    std::string ClientIntro::to_string() const
    {
        return "Intro[{}, hop={}, exp={}]"_format(
            relay.short_string(), hop.short_string(), expiry.time_since_epoch().count());
    }
}  //  namespace llarp
