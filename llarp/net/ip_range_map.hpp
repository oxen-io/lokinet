#ifndef LLARP_NET_IP_RANGE_MAP_HPP
#define LLARP_NET_IP_RANGE_MAP_HPP

#include <net/ip_range.hpp>
#include <list>

namespace llarp
{
  namespace net
  {
    /// a container that maps an ip range to a value that allows you to lookup
    /// key by range hit
    /// TODO: do some kind of magic shit to ensure near constant time for
    /// lookups
    template <typename Value_t>
    struct IPRangeMap
    {
      using Range_t = IPRange;
      using IP_t = Range_t::Addr_t;

      using Entry_t = std::pair<Range_t, Value_t>;
      using Container_t = std::list<Entry_t>;

      /// get a set of all values
      std::set<Value_t>
      Values() const
      {
        std::set<Value_t> all;
        for (const auto& entry : m_Entries)
          all.insert(entry.second);
        return all;
      }

      bool
      ContainsValue(const Value_t& val) const
      {
        for (const auto& entry : m_Entries)
        {
          if (entry.second == val)
            return true;
        }
        return false;
      }

      void
      ForEachValue(std::function<void(const Value_t&)> functor) const
      {
        for (const auto& entry : m_Entries)
          functor(entry.second);
      }

      template <typename Visit_t>
      void
      ForEachEntry(Visit_t visit) const
      {
        for (const auto& [range, value] : m_Entries)
          visit(range, value);
      }

      /// convert all values into type T using a transformer
      template <typename T, typename Transformer>
      std::set<T>
      TransformValues(Transformer transform) const
      {
        std::set<T> transformed;
        for (const auto& entry : m_Entries)
        {
          T val = transform(entry.second);
          transformed.insert(std::move(val));
        }
        return transformed;
      }

      /// return a set of all values who's range contains this IP
      std::set<Value_t>
      FindAll(const IP_t& addr) const
      {
        std::set<Value_t> found;
        for (const auto& entry : m_Entries)
        {
          if (entry.first.Contains(addr))
            found.insert(entry.second);
        }
        return found;
      }

      struct CompareEntry
      {
        bool
        operator()(const Entry_t& left, const Entry_t& right) const
        {
          return left.first < right.first;
        }
      };

      void
      Insert(const Range_t& addr, const Value_t& val)
      {
        m_Entries.emplace_front(addr, val);
        m_Entries.sort(CompareEntry{});
      }

      template <typename Visit_t>
      void
      RemoveIf(Visit_t visit)
      {
        auto itr = m_Entries.begin();
        while (itr != m_Entries.end())
        {
          if (visit(*itr))
            itr = m_Entries.erase(itr);
          else
            ++itr;
        }
      }

     private:
      Container_t m_Entries;
    };
  }  // namespace net
}  // namespace llarp

#endif
