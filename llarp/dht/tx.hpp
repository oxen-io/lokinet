#ifndef LLARP_DHT_TX
#define LLARP_DHT_TX

#include "key.hpp"
#include "txowner.hpp"
#include <llarp/util/logging/logger.hpp>
#include <llarp/util/status.hpp>

#include <set>
#include <vector>

namespace llarp
{
  namespace dht
  {
    struct AbstractContext;

    template <typename K, typename V>
    struct TX
    {
      K target;
      AbstractContext* parent;
      std::set<Key_t> peersAsked;
      std::vector<V> valuesFound;
      TXOwner whoasked;

      TX(const TXOwner& asker, const K& k, AbstractContext* p)
          : target(k), parent(p), whoasked(asker)
      {}

      virtual ~TX() = default;

      void
      OnFound(const Key_t& askedPeer, const V& value);

      util::StatusObject
      ExtractStatus() const
      {
        util::StatusObject obj{
            {"whoasked", whoasked.ExtractStatus()}, {"target", target.ExtractStatus()}};
        std::vector<util::StatusObject> foundObjs;
        std::transform(
            valuesFound.begin(),
            valuesFound.end(),
            std::back_inserter(foundObjs),
            [](const auto& item) -> util::StatusObject { return item.ExtractStatus(); });

        obj["found"] = foundObjs;
        std::vector<std::string> asked;
        std::transform(
            peersAsked.begin(),
            peersAsked.end(),
            std::back_inserter(asked),
            [](const auto& item) -> std::string { return item.ToString(); });
        obj["asked"] = asked;
        return obj;
      }

      virtual bool
      Validate(const V& value) const = 0;

      virtual void
      Start(const TXOwner& peer) = 0;

      virtual void
      SendReply() = 0;
    };

    template <typename K, typename V>
    inline void
    TX<K, V>::OnFound(const Key_t& askedPeer, const V& value)
    {
      peersAsked.insert(askedPeer);
      if (Validate(value))
      {
        valuesFound.push_back(value);
      }
    }
  }  // namespace dht
}  // namespace llarp

#endif
