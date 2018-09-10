#ifndef LLARP_DHT_BUCKET_HPP
#define LLARP_DHT_BUCKET_HPP

#include <llarp/dht/kademlia.hpp>
#include <llarp/dht/key.hpp>
#include <map>
#include <set>
#include <vector>

namespace llarp
{
  namespace dht
  {
    template < typename Val_t >
    struct Bucket
    {
      typedef std::map< Key_t, Val_t, XorMetric > BucketStorage_t;

      Bucket(const Key_t& us) : nodes(XorMetric(us)){};

      size_t
      Size() const
      {
        return nodes.size();
      }

      bool
      GetRandomNodeExcluding(Key_t& result,
                             const std::set< Key_t >& exclude) const
      {
        std::vector< Key_t > candidates;
        for(const auto& item : nodes)
        {
          if(exclude.find(item.first) == exclude.end())
            candidates.push_back(item.first);
        }
        if(candidates.size() == 0)
          return false;
        result = candidates[llarp_randint() % candidates.size()];
        return true;
      }

      bool
      FindClosest(const Key_t& target, Key_t& result) const
      {
        Key_t mindist;
        mindist.Fill(0xff);
        for(const auto& item : nodes)
        {
          auto curDist = item.first ^ target;
          if(curDist < mindist)
          {
            mindist = curDist;
            result  = item.first;
          }
        }
        return nodes.size() > 0;
      }

      bool
      GetManyRandom(std::set< Key_t >& result, size_t N) const
      {
        if(nodes.size() < N)
          return false;
        if(nodes.size() == N)
        {
          for(const auto& node : nodes)
          {
            result.insert(node.first);
          }
          return true;
        }
        size_t expecting = N;
        size_t sz        = nodes.size();
        while(N)
        {
          auto itr = nodes.begin();
          std::advance(itr, llarp_randint() % sz);
          if(result.insert(itr->first).second)
            --N;
        }
        return result.size() == expecting;
      }

      bool
      GetManyNearExcluding(const Key_t& target, std::set< Key_t >& result,
                           size_t N, const std::set< Key_t >& exclude) const
      {
        std::set< Key_t > s;
        for(const auto& k : exclude)
          s.insert(k);
        Key_t peer;
        while(N--)
        {
          if(!FindCloseExcluding(target, peer, s))
            return false;
          s.insert(peer);
          result.insert(peer);
        }
        return true;
      }

      bool
      FindCloseExcluding(const Key_t& target, Key_t& result,
                         const std::set< Key_t >& exclude) const
      {
        Key_t maxdist;
        maxdist.Fill(0xff);
        Key_t mindist;
        mindist.Fill(0xff);
        for(const auto& item : nodes)
        {
          if(exclude.count(item.first))
            continue;

          auto curDist = item.first ^ target;
          if(curDist < mindist)
          {
            mindist = curDist;
            result  = item.first;
          }
        }
        return mindist < maxdist;
      }

      void
      PutNode(const Val_t& val)
      {
        auto itr = nodes.find(val.ID);
        if(itr == nodes.end() || itr->second < val)
          nodes.insert(std::make_pair(val.ID, val));
      }

      void
      DelNode(const Key_t& key)
      {
        auto itr = nodes.find(key);
        if(itr != nodes.end())
          nodes.erase(itr);
      }

      BucketStorage_t nodes;
    };
  }  // namespace dht
}  // namespace llarp
#endif
