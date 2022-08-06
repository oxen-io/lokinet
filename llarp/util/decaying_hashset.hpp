#pragma once

#include "time.hpp"
#include <unordered_map>

namespace llarp
{
  namespace util
  {
    template <typename Val_t, typename Hash_t = std::hash<Val_t>>
    struct DecayingHashSet
    {
      using Time_t = std::chrono::milliseconds;
      /// the minimum number of generations between rehashing the underlying unordered map
      static inline constexpr int RehashGenerations = 10;

      bool nop{false};

      DecayingHashSet(Time_t item_lifetime = 1s)
          : m_CacheInterval{item_lifetime}
          , m_NextRehash{DateClock_t::now() + (m_CacheInterval * RehashGenerations)}
      {}

      size_t
      Size() const
      {
        return m_Values.size();
      }

      /// determine if we have v contained in our decaying hashset
      bool
      Contains(const Val_t& v) const
      {
        return m_Values.count(v) != 0;
      }

      /// return true if inserted
      /// return false if not inserted
      bool
      Insert(const Val_t& v, Time_t now = 0s)
      {
        if (nop)
          return true;
        if (now == 0s)
          now = llarp::time_now_ms();
        return m_Values.try_emplace(v, now).second;
      }

      /// upsert will insert or update a value with time as now
      void
      Upsert(const Val_t& v)
      {
        m_Values[v] = llarp::time_now_ms();
      }

      /// decay hashset entries
      void
      Decay(Time_t now = 0s)
      {
        if (now == 0s)
          now = llarp::time_now_ms();
        // decay
        const auto before = m_Values.size();
        EraseIf([&](const auto& item) { return (m_CacheInterval + item.second) <= now; });
        const auto after = m_Values.size();
        // rehash as needed
        const time_delta<std::chrono::milliseconds> time{m_NextRehash};
        if (before > after and time.delta() <= 0s)
        {
          m_Values.reserve(after);
          m_NextRehash = DateClock_t::now() + (m_CacheInterval * RehashGenerations);
        }
      }

      Time_t
      DecayInterval() const
      {
        return m_CacheInterval;
      }

      bool
      Empty() const
      {
        return m_Values.empty();
      }

      void
      DecayInterval(Time_t interval)
      {
        m_CacheInterval = interval;
      }

      void
      Remove(const Val_t& val)
      {
        m_Values.erase(val);
      }

     private:
      template <typename Predicate_t>
      void
      EraseIf(Predicate_t pred)
      {
        for (auto i = m_Values.begin(), last = m_Values.end(); i != last;)
        {
          if (pred(*i))
          {
            i = m_Values.erase(i);
          }
          else
          {
            ++i;
          }
        }
      }

      Time_t m_CacheInterval;
      TimePoint_t m_NextRehash;
      std::unordered_map<Val_t, Time_t, Hash_t> m_Values;
    };
  }  // namespace util
}  // namespace llarp
