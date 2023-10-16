#pragma once

#include "common.hpp"

namespace llarp
{
  namespace PathBuildMessage
  {
    inline auto OK = "OK"sv;
    inline auto EXCEPTION = "EXCEPTION"sv;
    inline auto BAD_FRAMES = "BAD_FRAMES"sv;
    inline auto BAD_CRYPTO = "BAD_CRYPTO"sv;
    inline auto NO_TRANSIT = "NOT ALLOWING TRANSIT"sv;
    inline auto BAD_PATHID = "BAD PATH ID"sv;
    inline auto BAD_LIFETIME = "BAD PATH LIFETIME (TOO LONG)"sv;

    inline static void
    setup_hop_keys(path::PathHopConfig& hop, const RouterID& nextHop)
    {
      auto crypto = CryptoManager::instance();

      // generate key
      crypto->encryption_keygen(hop.commkey);

      hop.nonce.Randomize();
      // do key exchange
      if (!crypto->dh_client(hop.shared, hop.rc.pubkey, hop.commkey, hop.nonce))
      {
        auto err = fmt::format("Failed to generate shared key for path build!");
        log::warning(path_cat, err);
        throw std::runtime_error{std::move(err)};
      }
      // generate nonceXOR value self->hop->pathKey
      crypto->shorthash(hop.nonceXOR, hop.shared.data(), hop.shared.size());

      hop.upstream = nextHop;
    }

    inline static std::string
    serialize(const path::PathHopConfig& hop)
    {
      auto crypto = CryptoManager::instance();

      std::string hop_info;

      {
        oxenc::bt_dict_producer btdp;

        btdp.append("COMMKEY", hop.commkey.toPublic().ToView());
        btdp.append("LIFETIME", path::DEFAULT_LIFETIME.count());
        btdp.append("NONCE", hop.nonce.ToView());
        btdp.append("RX", hop.rxID.ToView());
        btdp.append("TX", hop.txID.ToView());
        btdp.append("UPSTREAM", hop.upstream.ToView());

        hop_info = std::move(btdp).str();
      }

      SecretKey framekey;
      crypto->encryption_keygen(framekey);

      SharedSecret shared;
      TunnelNonce outer_nonce;
      outer_nonce.Randomize();

      // derive (outer) shared key
      if (!crypto->dh_client(shared, hop.rc.pubkey, framekey, outer_nonce))
      {
        log::warning(path_cat, "DH client failed during hop info encryption!");
        throw std::runtime_error{"DH failed during hop info encryption"};
      }

      // encrypt hop_info (mutates in-place)
      if (!crypto->xchacha20(
              reinterpret_cast<unsigned char*>(hop_info.data()),
              hop_info.size(),
              shared,
              outer_nonce))
      {
        log::warning(path_cat, "Hop info encryption failed!");
        throw std::runtime_error{"Hop info encryption failed"};
      }

      std::string hashed_data;

      {
        oxenc::bt_dict_producer btdp;

        btdp.append("ENCRYPTED", hop_info);
        btdp.append("NONCE", outer_nonce.ToView());
        btdp.append("PUBKEY", framekey.toPublic().ToView());

        hashed_data = std::move(btdp).str();
      }

      std::string hash;
      hash.reserve(SHORTHASHSIZE);

      if (!crypto->hmac(
              reinterpret_cast<uint8_t*>(hash.data()),
              reinterpret_cast<uint8_t*>(hashed_data.data()),
              hashed_data.size(),
              shared))
      {
        log::warning(path_cat, "Failed to generate HMAC for hop info");
        throw std::runtime_error{"Failed to generate HMAC for hop info"};
      }

      oxenc::bt_dict_producer btdp;

      btdp.append("HASH", hash);
      btdp.append("FRAME", hashed_data);

      return std::move(btdp).str();
    }
  }  // namespace PathBuildMessage

  namespace RelayCommitMessage
  {}

  namespace RelayStatusMessage
  {}

  namespace PathConfirmMessage
  {}

  namespace PathLatencyMessage
  {}

  namespace PathTransferMessage
  {}

}  // namespace llarp
