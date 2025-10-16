#include "contactdb.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/router/router.hpp>

namespace llarp
{
    static auto logcat = log::Cat("contactdb");

    ContactDB::ContactDB(Router& r) : _router{r} {}

    const EncryptedClientContact* ContactDB::get_encrypted_cc(const PubKey& blinded_key) const
    {
        if (auto it = _storage.find(blinded_key); it != _storage.end() && not it->second.is_expired())
            return &it->second;
        return nullptr;
    }

    size_t ContactDB::num_ccs() const { return _storage.size(); }

    void ContactDB::start_tickers()
    {
        _purge_ticker = _router.loop.call_every(30s, [this](){ purge_ccs(); }, true);
    }

    void ContactDB::purge_ccs(std::chrono::milliseconds now)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);

        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "ContactDB unable to continue purge ticking -- router is stopped!");
            return;
        }

        size_t removed = std::erase_if(_storage, [&now](const auto& c) { return c.second.is_expired(now); });
        if (removed)
            log::debug(logcat, "{} expired ClientContacts purged, {} remaining", removed, _storage.size());
        else
            log::trace(logcat, "No ClientContacts current expired (of {})", _storage.size());
    }

    void ContactDB::put_cc(EncryptedClientContact enc)
    {
        auto& current = _storage[enc.key()];
        if (enc.newer_than(current))
            current = std::move(enc);
    }

}  //  namespace llarp
