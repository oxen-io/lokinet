#ifndef LLARP_DHT_HPP_
#define LLARP_DHT_HPP_
#include <llarp/buffer.h>
#include <llarp/dht.h>
#include <llarp/router.h>
#include <llarp/router_contact.h>
#include <llarp/time.h>
#include <llarp/aligned.hpp>

#include <array>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace llarp
{
  namespace dht
  {
    const size_t MAX_MSG_SIZE = 2048;

    struct Key_t : public llarp::AlignedBuffer< 32 >
    {
      Key_t(const byte_t* val) : llarp::AlignedBuffer< 32 >(val)
      {
      }

      Key_t() : llarp::AlignedBuffer< 32 >()
      {
      }

      bool
      IsZero() const;

      Key_t
      operator^(const Key_t& other) const
      {
        Key_t dist;
        for(size_t idx = 0; idx < 4; ++idx)
          dist.l[idx] = l[idx] ^ other.l[idx];
        return dist;
      }

      bool
      operator<(const Key_t& other) const
      {
        return memcmp(data_l(), other.data_l(), 32) < 0;
      }
    };

    extern Key_t ZeroKey;

    struct Node
    {
      llarp_rc* rc;

      Key_t ID;

      Node() : rc(nullptr)
      {
        ID.Zero();
      }

      Node(llarp_rc* other) : rc(other)
      {
        ID = other->pubkey;
      }
    };

    struct SearchJob
    {
      const static uint64_t JobTimeout = 30000;

      SearchJob();

      SearchJob(const Key_t& requester, uint64_t requesterTX,
                const Key_t& target, llarp_router_lookup_job* job,
                const std::set< Key_t >& excludes);

      void
      Completed(const llarp_rc* router, bool timeout = false) const;

      bool
      IsExpired(llarp_time_t now) const;

      llarp_router_lookup_job* job = nullptr;
      llarp_time_t started;
      Key_t requester;
      uint64_t requesterTX;
      Key_t target;
      std::set< Key_t > exclude;
    };

    struct XorMetric
    {
      const Key_t& us;

      XorMetric(const Key_t& ourKey) : us(ourKey){};

      bool
      operator()(const Key_t& left, const Key_t& right) const
      {
        return (us ^ left) < (us ^ right);
      };
    };

    struct IMessage
    {
      virtual ~IMessage(){};

      IMessage(const Key_t& from) : From(from)
      {
      }

      virtual bool
      BEncode(llarp_buffer_t* buf) const = 0;

      virtual bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val) = 0;

      virtual bool
      HandleMessage(llarp_router* router,
                    std::vector< IMessage* >& replies) const = 0;

      Key_t From;
    };

    IMessage*
    DecodeMessage(const Key_t& from, llarp_buffer_t* buf);

    bool
    DecodeMesssageList(const Key_t& from, llarp_buffer_t* buf,
                       std::vector< IMessage* >& dst);

    struct Bucket
    {
      typedef std::map< Key_t, Node, XorMetric > BucketStorage_t;

      Bucket(const Key_t& us) : nodes(XorMetric(us)){};

      bool
      FindClosest(const Key_t& target, Key_t& result) const;

      bool
      FindCloseExcluding(const Key_t& target, Key_t& result,
                         const std::set< Key_t >& exclude) const;

      void
      PutNode(const Node& val);

      void
      DelNode(const Key_t& key);

      BucketStorage_t nodes;
    };

    struct Context
    {
      Context();
      ~Context();

      llarp_dht_msg_handler custom_handler = nullptr;

      SearchJob*
      FindPendingTX(const Key_t& owner, uint64_t txid);

      void
      RemovePendingLookup(const Key_t& owner, uint64_t txid);

      void
      LookupRouter(const Key_t& target, const Key_t& whoasked,
                   uint64_t whoaskedTX, const Key_t& askpeer,
                   llarp_router_lookup_job* job = nullptr,
                   bool iterative = false, std::set< Key_t > excludes = {});

      void
      LookupRouterViaJob(llarp_router_lookup_job* job);

      void
      LookupRouterRelayed(const Key_t& requester, uint64_t txid,
                          const Key_t& target, bool recursive,
                          std::vector< IMessage* >& replies);

      void
      Init(const Key_t& us, llarp_router* router);

      void
      QueueRouterLookup(llarp_router_lookup_job* job);

      static void
      handle_cleaner_timer(void* user, uint64_t orig, uint64_t left);

      static void
      queue_router_lookup(void* user);

      llarp_router* router = nullptr;
      Bucket* nodes        = nullptr;
      bool allowTransit    = false;

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
      };

      std::unordered_map< TXOwner, SearchJob, TXOwnerHash > pendingTX;
      Key_t ourKey;
    };

    struct GotRouterMessage : public IMessage
    {
      GotRouterMessage(const Key_t& from) : IMessage(from)
      {
      }
      GotRouterMessage(const Key_t& from, uint64_t id, const llarp_rc* result)
          : IMessage(from), txid(id)
      {
        if(result)
        {
          R.emplace_back();
          llarp_rc_clear(&R.back());
          llarp_rc_copy(&R.back(), result);
        }
      }

      ~GotRouterMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      HandleMessage(llarp_router* router,
                    std::vector< IMessage* >& replies) const;

      std::vector< llarp_rc > R;
      uint64_t txid    = 0;
      uint64_t version = 0;
    };

    struct FindRouterMessage : public IMessage
    {
      FindRouterMessage(const Key_t& from) : IMessage(from)
      {
      }

      FindRouterMessage(const Key_t& from, const Key_t& target, uint64_t id)
          : IMessage(from), K(target), txid(id)
      {
      }

      ~FindRouterMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      HandleMessage(llarp_router* router,
                    std::vector< IMessage* >& replies) const;

      Key_t K;
      bool iterative   = false;
      uint64_t txid    = 0;
      uint64_t version = 0;
    };
  }  // namespace dht
}  // namespace llarp

struct llarp_dht_context
{
  llarp::dht::Context impl;
  llarp_router* parent;
  llarp_dht_context(llarp_router* router);
};

#endif
