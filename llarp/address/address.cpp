#include "address.hpp"

#include <oxenc/base32z.h>

#include <stdexcept>

namespace llarp
{
    NetworkAddress::NetworkAddress(std::string_view arg)
    {
        if (arg.ends_with(TLD::SNODE))
        {
            _is_client = false;
            arg.remove_suffix(TLD::SNODE.size());
        }
        else if (arg.ends_with(TLD::LOKI))
        {
            _is_client = true;
            arg.remove_suffix(TLD::LOKI.size());
        }
        else
        {
            throw std::invalid_argument{
                "Invalid network address '{}': expected *{} or *{}"_format(arg, TLD::LOKI, TLD::SNODE)};
        }
        if (!_pubkey.from_base32z(arg))
            throw std::invalid_argument{"Invalid network address '{}{}': expected full pubkey"_format(
                arg, _is_client ? TLD::LOKI : TLD::SNODE)};
    }

    NetworkAddress::NetworkAddress(std::string_view arg, bool is_client) : _is_client{is_client}
    {
        if (!_pubkey.from_base32z(arg))
            throw std::invalid_argument{"Invalid pubkey passed to NetworkAddress constructor: {}"_format(arg)};
    }

    RelayAddress::RelayAddress(std::string_view arg)
    {
        if (not arg.ends_with(TLD::SNODE))
            throw std::invalid_argument{
                "Invalid RelayAddress constructor: {} does not end with '{}'"_format(arg, TLD::SNODE)};
        arg.remove_suffix(TLD::SNODE.size());
        if (!_pubkey.from_base32z(arg))
            throw std::invalid_argument{"Invalid pubkey passed to RelayAddress constructor: {}"_format(arg)};
    }

}  //  namespace llarp
