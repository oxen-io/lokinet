#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/findintro.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/routing/message.hpp>

namespace llarp
{
  namespace dht
  {
    struct IntroSetLookupInformer
    {
      llarp_router* router;
      service::Address target;

      void
      SendReply(const llarp::routing::IMessage* msg)
      {
      }
    };

    FindIntroMessage::~FindIntroMessage()
    {
    }

    bool
    FindIntroMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* val)
    {
      uint64_t i = 0;
      bool read  = false;
      if(BEncodeMaybeReadDictInt("I", i, read, k, val))
      {
        if(read)
        {
          iterative = i != 0;
        }
      }
      else
        return false;

      if(!BEncodeMaybeReadDictInt("R", R, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictEntry("S", S, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictInt("T", T, read, k, val))
        return false;

      if(!BEncodeMaybeReadVersion("V", version, LLARP_PROTO_VERSION, read, k,
                                  val))
        return false;

      return read;
    }

    bool
    FindIntroMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      // message id
      if(!BEncodeWriteDictMsgType(buf, "A", "F"))
        return false;
      // iterative
      if(!BEncodeWriteDictInt(buf, "I", iterative ? 1 : 0))
        return false;
      // r5n counter
      if(!BEncodeWriteDictInt(buf, "R", R))
        return false;
      // service address
      if(!BEncodeWriteDictEntry("S", S, buf))
        return false;
      // txid
      if(!BEncodeWriteDictInt(buf, "T", T))
        return false;
      // protocol version
      if(!BEncodeWriteDictInt(buf, "V", LLARP_PROTO_VERSION))
        return false;

      return bencode_end(buf);
    }

    bool
    FindIntroMessage::HandleMessage(
        llarp_dht_context* ctx,
        std::vector< llarp::dht::IMessage* >& replies) const
    {
      auto& dht           = ctx->impl;
      const auto introset = dht.GetIntroSetByServiceAddress(S);
      if(introset)
      {
        replies.push_back(new GotIntroMessage(T, introset));
        return true;
      }
      else
      {
        Key_t peer;
        std::set< Key_t > exclude = {dht.OurKey(), From};
        if(dht.nodes->FindCloseExcluding(S, peer, exclude))
        {
          dht.LookupIntroSet(S, From, 0, peer);
          return true;
        }
        else
        {
          llarp::LogError("cannot find closer peers for introset lookup for ",
                          S);
        }
      }
      return false;
    }

    RelayedFindIntroMessage::~RelayedFindIntroMessage()
    {
    }

    bool
    RelayedFindIntroMessage::HandleMessage(
        llarp_dht_context* ctx,
        std::vector< llarp::dht::IMessage* >& replies) const
    {
      auto& dht           = ctx->impl;
      const auto introset = dht.GetIntroSetByServiceAddress(S);
      if(introset)
      {
        replies.push_back(new GotIntroMessage(T, introset));
        return true;
      }
      else
      {
        Key_t peer;
        std::set< Key_t > exclude = {dht.OurKey()};
        if(dht.nodes->FindCloseExcluding(S, peer, exclude))
        {
          dht.LookupIntroSet(S, From, 0, peer);
          return true;
        }
        else
        {
          llarp::LogError("cannot find closer peers for introset lookup for ",
                          S);
        }
      }
      return false;
    }

  }  // namespace dht
}  // namespace llarp