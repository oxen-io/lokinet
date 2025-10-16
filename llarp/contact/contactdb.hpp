#pragma once

#include "client_contact.hpp"

namespace oxen::quic
{
    struct Ticker;
}

namespace llarp
{
    class Router;

    /**
        ContactDB TODO:
        - Store nearest-furthest expiry, trim
    */

    /// This class mediates storage, retrieval, and functionality for ClientContacts
    class ContactDB
    {
      private:
        Router& _router;

        std::unordered_map<PubKey, EncryptedClientContact, AlignedHasher> _storage;

        std::shared_ptr<quic::Ticker> _purge_ticker;

      public:
        explicit ContactDB(Router& r);

        const EncryptedClientContact* get_encrypted_cc(const PubKey& blinded_pk) const;

        void put_cc(EncryptedClientContact enc);

        void start_tickers();

        size_t num_ccs() const;

      private:
        void purge_ccs(std::chrono::milliseconds now = llarp::time_now_ms());
    };

}  // namespace llarp
