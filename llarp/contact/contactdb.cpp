#include "contactdb.hpp"

#include <llarp/router/router.hpp>

namespace llarp
{
    static auto logcat = log::Cat("contactdb");

    ContactDB::ContactDB(Router& r) : _router{r} {}

    std::optional<ClientContact> ContactDB::get_decrypted_cc(RouterID remote) const
    {
        if (auto* enc = get_encrypted_cc(hash_key::derive_from_rid(remote)))
            return enc->decrypt(remote);
        return std::nullopt;
    }

    const EncryptedClientContact* ContactDB::get_encrypted_cc(const hash_key& key) const
    {
        if (auto it = _storage.find(key); it != _storage.end() && not it->second.is_expired())
            return &it->second;
        return nullptr;
    }

    size_t ContactDB::num_ccs() const { return _storage.size(); }

    void ContactDB::start_tickers()
    {
        // FIXME: we do we delay this 5-10s?
        _router.loop()->call_later(uniform_duration_distribution{5s, 10s}(llarp::csrng), [this] {
            purge_ccs();
            log::trace(logcat, "ContactDB starting purge ticker..");
            _purge_ticker = _router.loop()->call_every(5min, [this] { purge_ccs(); });
        });
    }

    void ContactDB::purge_ccs(std::chrono::milliseconds now)
    {
        log::trace(logcat, "{} called", __PRETTY_FUNCTION__);
        assert(_router.loop()->inside());

        if (_router.is_stopping() || not _router.is_running())
        {
            log::debug(logcat, "ContactDB unable to continue purge ticking -- router is stopped!");
            return;
        }

        size_t n = 0;

        for (auto it = _storage.begin(); it != _storage.end();)
        {
            if (it->second.is_expired(now))
            {
                it = _storage.erase(it);
                n += 1;
            }
            else
                ++it;
        }

        if (n)
            log::debug(logcat, "{} expired ClientContacts purged!", n);
    }

    void ContactDB::put_cc(EncryptedClientContact enc)
    {
        auto& current = _storage[enc.key()];
        if (enc.newer_than(current))
            current = std::move(enc);
    }

}  //  namespace llarp
