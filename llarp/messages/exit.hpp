#pragma once

#include "common.hpp"

namespace llarp
{
  /*
      TODO:
        - change these parameters to ustringviews and ustrings where needed
  */

  namespace ObtainExit
  {
    inline auto EXCEPTION = "EXCEPTION"sv;

    // flag: 0 = Exit, 1 = Snode
    inline std::string
    sign_and_serialize(
        SecretKey sk, uint64_t flag, std::string pubkey, uint64_t seq_no, std::string tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');

      {
        auto btdp = btlp.append_dict();

        btdp.append("E", flag);
        btdp.append("I", pubkey);
        btdp.append("S", seq_no);
        btdp.append("T", tx_id);

        if (not CryptoManager::instance()->sign(
                reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: ObtainExitMessage failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }

    inline std::string
    sign_and_serialize_response(SecretKey sk, uint64_t seq_no, std::string_view tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');
      std::string nonce(16, '\0');
      randombytes(reinterpret_cast<uint8_t*>(nonce.data()), 16);

      {
        oxenc::bt_dict_producer btdp;

        btdp.append("S", seq_no);
        btdp.append("T", tx_id);
        btdp.append("Y", nonce);

        if (CryptoManager::instance()->sign(
                reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: ObtainExitMessage response failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }

  }  // namespace ObtainExit

  namespace UpdateExit
  {
    inline auto EXCEPTION = "EXCEPTION"sv;
    inline auto UPDATE_FAILED = "EXIT UPDATE FAILED"sv;

    inline std::string
    sign_and_serialize(SecretKey sk, std::string path_id, std::string seq_no, std::string tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');

      {
        auto btdp = btlp.append_dict();

        btdp.append("P", path_id);
        btdp.append("S", seq_no);
        btdp.append("T", tx_id);

        if (not CryptoManager::instance()->sign(
                reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: UpdateExitMessage failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }

    inline std::string
    sign_and_serialize_response(SecretKey sk, uint64_t seq_no, std::string_view tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');
      std::string nonce(16, '\0');
      randombytes(reinterpret_cast<uint8_t*>(nonce.data()), 16);

      {
        oxenc::bt_dict_producer btdp;

        btdp.append("S", seq_no);
        btdp.append("T", tx_id);
        btdp.append("Y", nonce);

        if (CryptoManager::instance()->sign(
                reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: UpdateExitMessage response failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }
  }  // namespace UpdateExit

  namespace CloseExit
  {
    inline auto EXCEPTION = "EXCEPTION"sv;
    inline auto UPDATE_FAILED = "CLOSE EXIT FAILED"sv;

    inline std::string
    sign_and_serialize(SecretKey sk, std::string seq_no, std::string tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');
      std::string nonce(16, '\0');
      randombytes(reinterpret_cast<uint8_t*>(nonce.data()), 16);

      {
        auto btdp = btlp.append_dict();

        btdp.append("S", seq_no);
        btdp.append("T", tx_id);
        btdp.append("Y", nonce);

        if (not CryptoManager::instance()->sign(
                reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: CloseExitMessage failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }

    inline std::string
    sign_and_serialize_response(SecretKey sk, uint64_t seq_no, std::string_view tx_id)
    {
      oxenc::bt_list_producer btlp;
      std::string sig(64, '\0');
      std::string nonce(16, '\0');
      randombytes(reinterpret_cast<uint8_t*>(nonce.data()), 16);

      {
        oxenc::bt_dict_producer btdp;

        btdp.append("S", seq_no);
        btdp.append("T", tx_id);
        btdp.append("Y", nonce);

        if (CryptoManager::instance()->sign(
                reinterpret_cast<uint8_t*>(sig.data()), sk, to_usv(btdp.view())))
          throw std::runtime_error{
              "Error: CloseExitMessage response failed to sign and serialize contents!"};
      }

      btlp.append(sig.data());
      return std::move(btlp).str();
    }
  }  // namespace CloseExit

}  // namespace llarp
