#pragma once

#include <windows.h>
#include <fwpmu.h>
#include <fwptypes.h>

#include "width.hpp"

#include <llarp/net/net_int.hpp>

namespace llarp::win32
{
  static inline GUID
  MakeGUID(std::string str)
  {
    GUID guid{};
    if (UuidFromString(reinterpret_cast<RPC_CSTR>(str.data()), &guid) != RPC_S_OK)
      throw std::invalid_argument{"invalid guid: " + str};
    return guid;
  }

  /// these values were lifted from wireguad windows port:
  /// https://github.com/WireGuard/wireguard-windows/blob/master/tunnel/firewall/types_windows.go
  /// they are kind of undocumented for whatever reason.

  static inline GUID
  FWPM_LAYER_OUTBOUND_IPPACKET_V6()
  {
    return MakeGUID("a3b42c97-9f04-4672-b87e-cee9c483257f");
  }

  static inline GUID
  FWPM_LAYER_OUTBOUND_IPPACKET_V4()
  {
    return MakeGUID("e1cd9fe7-f4b5-4273-96c0-592e487b8650");
  }

  static inline GUID
  FWPM_CONDITION_IP_LOCAL_INTERFACE()
  {
    return MakeGUID("4cd62a49-59c3-4969-b7f3-bda5d32890a4");
  }

  static inline GUID
  FWPM_CONDITION_IP_REMOTE_ADDRESS()
  {
    return MakeGUID("b235ae9a-1d64-49b8-a44c-5ff3d9095045");
  }

  static inline GUID
  FWPM_CONDITION_IP_LOCAL_PORT()
  {
    return MakeGUID("0c1ba1af-5765-453f-af22-a8f791ac775b");
  }

  static inline GUID
  FWPM_CONDITION_IP_REMOTE_PORT()
  {
    return MakeGUID("c35a604d-d22b-4e1a-91b4-68f674ee674b");
  }

  static inline GUID
  FWPM_LAYER_ALE_AUTH_CONNECT_V4()
  {
    return MakeGUID("c38d57d1-05a7-4c33-904f-7fbceee60e82");
  }

  static inline GUID
  FWPM_LAYER_ALE_AUTH_CONNECT_V6()
  {
    return MakeGUID("4a72393b-319f-44bc-84c3-ba54dcb3b6b4");
  }

  static inline GUID
  FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4()
  {
    return MakeGUID("e1cd9fe7-f4b5-4273-96c0-592e487b8650");
  }

  static inline GUID
  FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6()
  {
    return MakeGUID("a3b42c97-9f04-4672-b87e-cee9c483257f");
  }

  static inline GUID
  FWPM_CONDITION_FLAGS()
  {
    return MakeGUID("632ce23b-5167-435c-86d7-e903684aa80c");
  }

  static inline GUID
  FWPM_CONDITION_IP_PROTOCOL()
  {
    return MakeGUID("3971ef2b-623e-4f9a-8cb1-6e79b806b9a7");
  }

  template <typename Value_t>
  static inline FWP_CONDITION_VALUE0_ MakeValue(Value_t);

  template <typename Value_t>
  static inline FWPM_FILTER_CONDITION0_
  MakeCondition(GUID key, FWP_MATCH_TYPE matchType, Value_t val)
  {
    FWPM_FILTER_CONDITION0_ condition{};
    condition.fieldKey = key;
    condition.matchType = matchType;
    condition.conditionValue = MakeValue<Value_t>(val);
    return condition;
  }

  template <>
  inline FWP_CONDITION_VALUE0_
  MakeValue(uint8_t val)
  {
    FWP_CONDITION_VALUE0_ _ret{};
    _ret.type = FWP_UINT8;
    _ret.uint8 = val;
    return _ret;
  }

  template <>
  inline FWP_CONDITION_VALUE0_
  MakeValue(huint16_t val)
  {
    FWP_CONDITION_VALUE0_ _ret{};
    _ret.type = FWP_UINT16;
    _ret.uint16 = val.h;
    return _ret;
  }

  template <>
  inline FWP_CONDITION_VALUE0_
  MakeValue(huint32_t val)
  {
    FWP_CONDITION_VALUE0_ _ret{};
    _ret.type = FWP_UINT32;
    _ret.uint32 = val.h;
    return _ret;
  }

  template <>
  inline FWP_CONDITION_VALUE0_
  MakeValue(uint64_t* val)
  {
    FWP_CONDITION_VALUE0_ _ret{};
    _ret.type = FWP_UINT64;
    _ret.uint64 = val;
    return _ret;
  }
}  // namespace llarp::win32
