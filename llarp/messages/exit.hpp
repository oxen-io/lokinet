#pragma once

#include "common.hpp"

namespace llarp
{
  /*
      TODO:
        - change these parameters to ustringviews and ustrings where needed after bumping oxenc
        - change std::string sig(64, '\0') --> std::array<unsigned char, 64> sig
  */

  namespace ObtainExitMessage
  {

    // flag: 0 = Exit, 1 = Snode
    inline std::string
    sign_and_serialize(SecretKey sk, uint64_t flag, std::string tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');

      {
        auto btdp = btlp.append_dict();

        btdp.append("E", flag);
        btdp.append("T", tx_id);

        if (not crypto::sign(reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: ObtainExitMessage failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }

    inline std::string
    sign_and_serialize_response(SecretKey sk, std::string_view tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');
      std::string nonce(16, '\0');
      randombytes(reinterpret_cast<uint8_t*>(nonce.data()), 16);

      {
        oxenc::bt_dict_producer btdp;

        btdp.append("T", tx_id);
        btdp.append("Y", nonce);

        if (crypto::sign(reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: ObtainExitMessage response failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }

  }  // namespace ObtainExitMessage

  namespace UpdateExitMessage
  {
    inline auto UPDATE_FAILED = "EXIT UPDATE FAILED"sv;

    inline std::string
    sign_and_serialize(SecretKey sk, std::string path_id, std::string tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');

      {
        auto btdp = btlp.append_dict();

        btdp.append("P", path_id);
        btdp.append("T", tx_id);

        if (not crypto::sign(reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: UpdateExitMessage failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }

    inline std::string
    sign_and_serialize_response(SecretKey sk, std::string_view tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');
      std::string nonce(16, '\0');
      randombytes(reinterpret_cast<uint8_t*>(nonce.data()), 16);

      {
        oxenc::bt_dict_producer btdp;

        btdp.append("T", tx_id);
        btdp.append("Y", nonce);

        if (crypto::sign(reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: UpdateExitMessage response failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }
  }  // namespace UpdateExitMessage

  namespace CloseExitMessage
  {
    inline auto UPDATE_FAILED = "CLOSE EXIT FAILED"sv;

    inline std::string
    sign_and_serialize(SecretKey sk, std::string tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');
      std::string nonce(16, '\0');
      randombytes(reinterpret_cast<uint8_t*>(nonce.data()), 16);

      {
        auto btdp = btlp.append_dict();

        btdp.append("T", tx_id);
        btdp.append("Y", nonce);

        if (not crypto::sign(reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: CloseExitMessage failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }

    inline std::string
    sign_and_serialize_response(SecretKey sk, std::string_view tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');
      std::string nonce(16, '\0');
      randombytes(reinterpret_cast<uint8_t*>(nonce.data()), 16);

      {
        oxenc::bt_dict_producer btdp;

        btdp.append("T", tx_id);
        btdp.append("Y", nonce);

        if (crypto::sign(reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: CloseExitMessage response failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }
  }  // namespace CloseExitMessage

}  // namespace llarp
