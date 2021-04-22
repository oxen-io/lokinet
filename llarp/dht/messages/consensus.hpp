#pragma once
#include <llarp/dht/message.hpp>
#include <llarp/router_version.hpp>

namespace llarp
{
  namespace dht
  {
    struct ConsensusMessage
    {
      /// H
      ShortHash m_Hash;
      /// K
      std::vector<RouterID> m_Keys;
      /// N
      uint64_t m_NumberOfEntries;
      /// O
      uint64_t m_EntryOffset;
      /// T
      uint64_t m_TxID;
      /// U
      llarp_time_t m_NextUpdateRequired;
      /// V
      RouterVersion m_RotuerVersion;
    };
  }  // namespace dht
}  // namespace llarp
