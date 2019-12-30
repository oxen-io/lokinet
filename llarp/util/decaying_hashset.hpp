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
      DecayingHashSet(llarp_time_t cacheInterval = 5000)
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
      Insert(const Val_t& v, llarp_time_t now = 0)
      {
        if(Contains(v))
          return false;
        if(now == 0)
          now = llarp::time_now_ms();
        m_Values.emplace(v, now + m_CacheInterval);
        return true;
      }

      /// decay hashset entries
      void
      Decay(llarp_time_t now = 0)
      {
        if(now == 0)
          now = llarp::time_now_ms();
        auto itr = m_Values.begin();
        while(itr != m_Values.end())
        {
          if(itr->second <= now)
            itr = m_Values.erase(itr);
          else
            ++itr;
        }
      }

      llarp_time_t
      DecayInterval() const
      {
        return m_CacheInterval;
      }

     private:
      const llarp_time_t m_CacheInterval;
      std::unordered_map< Val_t, llarp_time_t, Hash_t > m_Values;
    };
  }  // namespace util
}  // namespace llarp

#endif
