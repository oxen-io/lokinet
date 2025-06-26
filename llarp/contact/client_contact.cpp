#include "client_contact.hpp"

#include <oxenc/bt_serialize.h>

#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>

#include <type_traits>

namespace llarp
{
    static auto logcat = log::Cat("client-intro");

    ClientContact::ClientContact(
        Ed25519PrivateData private_data,
        PubKey pk,
        std::unordered_set<dns::SRVData> srvs,
        protocol_flag protocols,
        std::optional<net::ExitPolicy> policy)
        : derived_privatekey{std::move(private_data)},
          _pubkey{std::move(pk)},
          _srv{std::move(srvs)},
          _protos{protocols},
          _exit_policy{std::move(policy)}
    {}

    ClientContact::ClientContact(std::span<const unsigned char> buf) { bt_decode(oxenc::bt_dict_consumer{buf}); }

    void ClientContact::update_intros(sorted_intro_set iset)
    {
        if (iset.empty())
            throw std::invalid_argument{"Cannot publish ClientContact with no ClientIntros!"};
        _intros = std::move(iset);
        log::debug(logcat, "ClientContact stored updated ClientIntros (n={})...", _intros.size());
    }

#ifdef __cpp_lib_to_underlying
    using std::to_underlying;
#else
    template <class Enum>
    constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept
    {
        return static_cast<std::underlying_type_t<Enum>>(e);
    }
#endif

    std::vector<unsigned char> ClientContact::bt_encode() const
    {
        oxenc::bt_dict_producer btdp;
        btdp.append<uint8_t>("", VERSION);

        btdp.append("a", _pubkey.to_view());

        if (_exit_policy)
            _exit_policy->bt_encode(btdp.append_dict("e"));

        {
            auto sublist = btdp.append_list("i");
            for (auto& i : _intros)
                i.bt_encode(sublist.append_dict());
        }

        btdp.append("p", to_underlying(_protos));

        if (not _srv.empty())
        {
            auto sublist = btdp.append_list("s");
            for (auto& s : _srv)
                s.bt_encode(sublist.append_dict());
        }

        auto encoded = btdp.view();
        std::vector<unsigned char> ret;
        ret.resize(encoded.size());
        std::memcpy(ret.data(), encoded.data(), encoded.size());
        return ret;
    }

    void ClientContact::bt_decode(oxenc::bt_dict_consumer&& btdc)
    {
        auto version = btdc.require<uint8_t>("");

        if (version != VERSION)
            throw std::runtime_error{
                "Deserialized ClientContact with unsupported version {} (expected {})!"_format(version, VERSION)};

        _pubkey.from_string(btdc.require<std::string_view>("a"));

        if (btdc.skip_until("e"))
            _exit_policy.emplace().bt_decode(btdc.consume_dict_consumer());

        for (auto sublist = btdc.require<oxenc::bt_list_consumer>("i"); not sublist.is_finished();)
            _intros.emplace(sublist.consume_dict_consumer());

        _protos = static_cast<protocol_flag>(btdc.require<std::underlying_type_t<protocol_flag>>("p"));

        if (auto sublist = btdc.maybe<oxenc::bt_list_consumer>("s"))
            while (not sublist->is_finished())
                _srv.emplace(sublist->consume_dict_consumer());
    }

    session_tag ClientContact::generate_session_tag() const { return session_tag{_protos}; }

    bool ClientContact::is_expired(std::chrono::milliseconds now) const
    {
        // We want to check the first one, because this is sorted newest-to-oldest
        auto it = _intros.begin();
        if (it == _intros.end())
            return true;
        return it->is_expired(now);
    }

    EncryptedClientContact ClientContact::encrypt_and_sign() const
    {
        EncryptedClientContact enc{};

        try
        {
            enc.blinded_pubkey.assign(derived_privatekey.to_pubkey().span());
            enc.encrypted = bt_encode();

            if (not crypto::xchacha20(enc.encrypted.data(), enc.encrypted.size(), _pubkey.data(), enc.nonce.data()))
                throw std::runtime_error{"Failed to encrypt ClientContact bt-payload!"};

            enc.signed_at = llarp::time_now_ms();

            auto btdp = enc.bt_encode_for_signing();
            btdp.append_signature("~", [&enc, this](std::span<const uint8_t> to_sign) {
                if (not crypto::sign(enc.sig, derived_privatekey, to_sign.data(), to_sign.size()))
                    throw std::runtime_error{"Failed to sign EncryptedClientContact payload!"};
                return enc.sig.span();
            });

            enc._bt_payload = std::move(btdp).str();
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception encrypting and signing client contact: {}", e.what());
            throw;
        }

        return enc;
    }

    std::string ClientContact::to_string() const
    {
        return "CC[{}{}, {}, {} intros]"_format(
            _pubkey.short_string(), _exit_policy ? ", exit" : "", _intros.size(), llarp::to_string(_protos));
    }

    EncryptedClientContact::EncryptedClientContact(std::string_view buf) : _bt_payload{buf}
    {
        bt_decode(oxenc::bt_dict_consumer{_bt_payload});
    }

    /** EncryptedClientContact
            "i" blinded local routerID
            "n" nounce
            "t" signing time
            "x" encrypted payload
            "~" signature
    */
    void EncryptedClientContact::bt_decode(oxenc::bt_dict_consumer&& btdc)
    {
        try
        {
            blinded_pubkey.from_string(btdc.require<std::string_view>("i"));
            nonce.from_string(btdc.require<std::string_view>("n"));
            signed_at = std::chrono::milliseconds{btdc.require<uint64_t>("t")};

            // TESTNET: TOFIX: change this after oxenc span PR is merged
            auto enc = btdc.require<std::string_view>("x");
            encrypted.resize(enc.size());
            std::memcpy(encrypted.data(), enc.data(), enc.size());

            sig.from_string(btdc.require<std::string_view>("~"));
        }
        catch (const std::exception& e)
        {
            log::critical(
                logcat,
                "EncryptedClientContact deserialization failed: {} : payload: {}",
                e.what(),
                buffer_printer{_bt_payload});
            throw;
        }
    }

    std::optional<ClientContact> EncryptedClientContact::decrypt(const PubKey& root) const
    {
        std::optional<ClientContact> cc;
        auto plaintext = encrypted;
        if (crypto::xchacha20(
                reinterpret_cast<unsigned char*>(plaintext.data()), plaintext.size(), root.data(), nonce.data()))
        {
            log::debug(logcat, "EncryptedClientContact decrypted successfully...");
            cc.emplace(plaintext);
        }
        else
            log::warning(logcat, "Failed to decrypt EncryptedClientContact!");

        return cc;
    }

    bool EncryptedClientContact::verify() const
    {
        try
        {
            oxenc::bt_dict_consumer btdc{_bt_payload};

            btdc.require_signature("~", [this](std::span<const uint8_t> m, std::span<const uint8_t> s) {
                if (s.size() != 64)
                    throw std::runtime_error{"Invalid signature: not 64 bytes"};

                if (not crypto::verify(blinded_pubkey, m, s))
                    throw std::runtime_error{"Failed to verify EncryptedClientContact signature!"};
            });
        }
        catch (const std::exception& e)
        {
            log::warning(logcat, "Exception: {}", e.what());
            return false;
        }

        log::trace(logcat, "Successfully verified EncryptedClientContact!");

        return true;
    }

    bool EncryptedClientContact::is_expired(std::chrono::milliseconds now) const
    {
        return now >= signed_at + path::DEFAULT_LIFETIME;
    }
}  //  namespace llarp
