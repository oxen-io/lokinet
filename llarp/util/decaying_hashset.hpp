#ifndef LLARP_UTIL_DECAYING_HASHSET_HPP
#define LLARP_UTIL_DECAYING_HASHSET_HPP

#include <util/time.hpp>
#include <unordered_map>

namespace llarp
{
  namespace util
  {
    template < typename Val_t, typename Hash_t = typename Val_t::Hash >
    struct DecayingHashSet
    {
      using Time_t = std::chrono::milliseconds;

      DecayingHashSet(Time_t cacheInterval = 5s)
          : m_CacheInterval(cacheInterval)
      {
      }
      /// determine if we have v contained in our decaying hashset
      bool
      Contains(const Val_t& v) const
      {
        return m_Values.find(v) != m_Values.end();
      }

      /// return true if inserted
      /// return false if not inserted
      bool
      Insert(const Val_t& v, Time_t now = 0s)
      {
        if(now == 0s)
          now = llarp::time_now_ms();
        return m_Values.emplace(v, now).second;
      }

      /// decay hashset entries
      void
      Decay(Time_t now = 0s)
      {
        if(now == 0s)
          now = llarp::time_now_ms();

        auto itr = m_Values.begin();
        while(itr != m_Values.end())
        {
          if((m_CacheInterval + itr->second) <= now)
            itr = m_Values.erase(itr);
          else
            ++itr;
        }
      }

      Time_t
      DecayInterval() const
      {
        return m_CacheInterval;
      }

      void
      DecayInterval(Time_t interval)
      {
        m_CacheInterval = interval;
      }

     private:
      Time_t m_CacheInterval;
      std::unordered_map< Val_t, Time_t, Hash_t > m_Values;
    };
  }  // namespace util
}  // namespace llarp

#endif
