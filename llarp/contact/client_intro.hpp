#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/path/path_types.hpp>

#include <oxenc/bt.h>

namespace llarp
{
    struct ClientIntro
    {
        RouterID pivot_rid;
        HopID pivot_txid;
        std::chrono::milliseconds expiry{0s};

        ClientIntro() = default;
        ClientIntro(oxenc::bt_dict_consumer&&);
        ClientIntro(std::string_view buf);

        std::chrono::milliseconds expires_in(std::chrono::milliseconds now = llarp::time_now_ms()) const
        {
            return expiry - now;
        }

        bool is_expired(std::chrono::milliseconds now = llarp::time_now_ms()) const { return expires_in(now) <= 0ms; }

        void bt_encode(oxenc::bt_dict_producer&& subdict) const;

        // Does not throw, returns true/false
        bool bt_decode(std::string_view buf);

        bool operator==(const ClientIntro& other) const = default;

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;

      protected:
        // Throws if unsuccessful, must take BTDC in invocation
        void bt_decode(oxenc::bt_dict_consumer&& btdc);
    };

}  //  namespace llarp
