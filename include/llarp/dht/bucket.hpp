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

      bool
      GetRandomNodeExcluding(Key_t& result, std::set< Key_t > exclude) const
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
      FindCloseExcluding(const Key_t& target, Key_t& result,
                         std::set< Key_t > exclude) const
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