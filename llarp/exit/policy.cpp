#include "policy.hpp"

namespace llarp::exit
{
    void Policy::bt_encode(oxenc::bt_dict_producer& btdp) const
    {
        try
        {
            btdp.append("a", proto);
            btdp.append("b", port);
            btdp.append("d", drop);
            btdp.append("v", version);
        }
        catch (...)
        {
            log::critical(policy_cat, "Error: exit Policy failed to bt encode contents!");
        }
    }

    bool Policy::decode_key(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
        bool read = false;
        if (!BEncodeMaybeReadDictInt("a", proto, read, k, buf))
            return false;
        if (!BEncodeMaybeReadDictInt("b", port, read, k, buf))
            return false;
        if (!BEncodeMaybeReadDictInt("d", drop, read, k, buf))
            return false;
        if (!BEncodeMaybeReadDictInt("v", version, read, k, buf))
            return false;
        return read;
    }

}  // namespace llarp::exit
