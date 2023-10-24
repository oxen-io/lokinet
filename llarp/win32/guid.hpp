#pragma once

#include <llarp/crypto/crypto.hpp>

#include <windows.h>

#include <type_traits>

namespace llarp::win32
{
  /// @brief given a container of data hash it and make it into a GUID so we have a way to
  /// deterministically generate GUIDs
  template <typename Data>
  inline GUID
  MakeDeterministicGUID(Data data)
  {
    ShortHash h{};
    auto hash = [&h](auto data) { crypto::shorthash(h, data); };

    if constexpr (std::is_same_v<Data, std::string>)
      hash(llarp_buffer_t{reinterpret_cast<const byte_t*>(data.data()), data.size()});
    else
      hash(llarp_buffer_t{data});
    GUID guid{};
    std::copy_n(
        h.begin(), std::min(sizeof(GUID), sizeof(ShortHash)), reinterpret_cast<uint8_t*>(&guid));
    return guid;
  }
}  // namespace llarp::win32
