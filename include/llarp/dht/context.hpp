#ifndef LLARP_DHT_CONTEXT_HPP
#define LLARP_DHT_CONTEXT_HPP

#include <llarp/dht.h>
#include <llarp/router.h>
#include <llarp/dht/bucket.hpp>
#include <llarp/dht/key.hpp>
#include <llarp/dht/message.hpp>
#include <llarp/dht/node.hpp>
#include <llarp/dht/search_job.hpp>
#include <llarp/service/IntroSet.hpp>

#include <set>

namespace llarp
{
  namespace dht
  {
    struct Context
    {
      Context();
      ~Context();

      SearchJob*
      FindPendingTX(const Key_t& owner, uint64_t txid);

      void
      RemovePendingLookup(const Key_t& owner, uint64_t txid);

      void
      LookupServiceDirect(const Key_t& target, const Key_t& whoasked,
                          uint64_t whoaskedTX, const Key_t& askpeer,
                          SearchJob::IntroSetHookFunc handler,
                          bool iterateive            = false,
                          std::set< Key_t > excludes = {});

      void
      LookupRouter(const Key_t& target, const Key_t& whoasked,
                   uint64_t whoaskedTX, const Key_t& askpeer,
                   llarp_router_lookup_job* job = nullptr,
                   bool iterative = false, std::set< Key_t > excludes = {});

      void
      LookupIntroSet(const service::Address& addr, const Key_t& whoasked,
                     uint64_t whoaskedTX, const Key_t& askpeer,
                     bool interative = false, std::set< Key_t > excludes = {});

      void
      LookupTag(const service::Tag& tag, const Key_t& whoasked,
                uint64_t whoaskedTX, const Key_t& askpeer,
                const std::set< service::IntroSet >& include = {},
                uint64_t R                                   = 0);

      void
      LookupRouterViaJob(llarp_router_lookup_job* job);

      void
      LookupTagForPath(const service::Tag& tag, uint64_t txid,
                       const llarp::PathID_t& path, const Key_t& askpeer);

      void
      LookupIntroSetForPath(const service::Address& addr, uint64_t txid,
                            const llarp::PathID_t& path, const Key_t& askpeer);

      void
      LookupIntroSetRelayed(const Key_t& requester, uint64_t txid,
                            const service::Address& addr, bool recursive,
                            std::vector< IMessage* >& reply);

      std::set< service::IntroSet >
      FindRandomIntroSetsWithTag(const service::Tag& tag, size_t max = 2);

      void
      LookupRouterRelayed(const Key_t& requester, uint64_t txid,
                          const Key_t& target, bool recursive,
                          std::vector< IMessage* >& replies);

      bool
      RelayRequestForPath(const llarp::PathID_t& localPath,
                          const IMessage* msg);

      void
      PropagateIntroSetTo(const Key_t& from, uint64_t fromTX,
                          const service::IntroSet& introset, const Key_t& peer,
                          uint64_t S, const std::set< Key_t >& exclude);

      void
      Init(const Key_t& us, llarp_router* router);

      const llarp::service::IntroSet*
      GetIntroSetByServiceAddress(const llarp::service::Address& addr) const;

      void
      QueueRouterLookup(llarp_router_lookup_job* job);

      static void
      handle_cleaner_timer(void* user, uint64_t orig, uint64_t left);

      static void
      queue_router_lookup(void* user);

      llarp_router* router = nullptr;
      // for router contacts
      Bucket< RCNode >* nodes = nullptr;

      // for introduction sets
      Bucket< ISNode >* services = nullptr;
      bool allowTransit          = false;

      const Key_t&
      OurKey() const
      {
        return ourKey;
      }

     private:
      void
      ScheduleCleanupTimer();

      void
      CleanupTX();

      uint64_t ids;

      struct TXOwner
      {
        Key_t node;
        uint64_t txid = 0;

        bool
        operator==(const TXOwner& other) const
        {
          return txid == other.txid && node == other.node;
        }
        bool
        operator<(const TXOwner& other) const
        {
          return txid < other.txid || node < other.node;
        }
      };

      struct TXOwnerHash
      {
        std::size_t
        operator()(TXOwner const& o) const noexcept
        {
          std::size_t sz2;
          memcpy(&sz2, &o.node[0], sizeof(std::size_t));
          return o.txid ^ (sz2 << 1);
        }
      };  // namespace dht

      std::unordered_map< TXOwner, SearchJob, TXOwnerHash > pendingTX;
      Key_t ourKey;
    };  // namespace llarp
  }     // namespace dht
}  // namespace llarp

struct llarp_dht_context
{
  llarp::dht::Context impl;
  llarp_router* parent;
  llarp_dht_context(llarp_router* router);
};

#endif