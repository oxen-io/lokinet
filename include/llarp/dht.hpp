#ifndef LLARP_DHT_HPP_
#define LLARP_DHT_HPP_
#include <llarp/buffer.h>
#include <llarp/dht.h>
#include <llarp/router.h>
#include <llarp/router_contact.h>
#include <llarp/aligned.hpp>

#include <array>
#include <map>
#include <vector>

namespace llarp
{
  namespace dht
  {
    const size_t MAX_MSG_SIZE = 2048;

    struct SearchJob;

    struct Node
    {
      llarp_rc rc;

      const byte_t*
      ID() const;

      Node();
      ~Node();
    };

    struct Key_t : public llarp::AlignedBuffer< 32 >
    {
      Key_t(const byte_t* val) : llarp::AlignedBuffer< 32 >(val)
      {
      }

      Key_t() : llarp::AlignedBuffer< 32 >()
      {
      }

      Key_t
      operator^(const Key_t& other) const
      {
        Key_t dist;
        for(size_t idx = 0; idx < 8; ++idx)
          dist.data_l()[idx] = data_l()[idx] ^ other.data_l()[idx];
        return dist;
      }

      bool
      operator<(const Key_t& other) const
      {
        return memcmp(data_l(), other.data_l(), 32) < 0;
      }
    };

    struct XorMetric
    {
      const Key_t& us;

      XorMetric(const Key_t& ourKey) : us(ourKey){};

      bool
      operator()(const Key_t& left, const Key_t& right) const
      {
        return (us ^ left) < right;
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

      BucketStorage_t nodes;
    };

    struct Context
    {
      Context();
      ~Context();

      llarp_dht_msg_handler custom_handler = nullptr;

      void
      Init(const Key_t& us);

     private:
      Bucket* nodes = nullptr;
      Key_t ourKey;
    };

    struct GotRouterMessage : public IMessage
    {
      GotRouterMessage(const Key_t& from) : IMessage(from)
      {
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

      ~FindRouterMessage();

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* val);

      bool
      HandleMessage(llarp_router* router,
                    std::vector< IMessage* >& replies) const;

      Key_t K;
      uint64_t txid    = 0;
      uint64_t version = 0;
    };
  }
}

struct llarp_dht_context
{
  llarp::dht::Context impl;
};

#endif
