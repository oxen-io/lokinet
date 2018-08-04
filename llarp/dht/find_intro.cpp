#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/findintro.hpp>
#include <llarp/dht/messages/gotintro.hpp>
#include <llarp/routing/message.hpp>

namespace llarp
{
  namespace dht
  {
    /*
    struct IntroSetLookupInformer
    {
      llarp_router* router;
      service::Address target;

      void
      SendReply(const llarp::routing::IMessage* msg)
      {
      }
    };
    */

    FindIntroMessage::~FindIntroMessage()
    {
    }

    bool
    FindIntroMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* val)
    {
      uint64_t i = 0;
      bool read  = false;
      if(!BEncodeMaybeReadDictInt("I", i, read, k, val))
        return false;
      if(read)
      {
        iterative = i != 0;
        return true;
      }

      if(!BEncodeMaybeReadDictEntry("N", N, read, k, val))
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
      if(!BEncodeWriteDictInt("I", iterative ? 1 : 0, buf))
        return false;
      if(N.IsZero())
      {
        return false;
        // r5n counter
        if(!BEncodeWriteDictInt("R", R, buf))
          return false;
        // service address
        if(!BEncodeWriteDictEntry("S", S, buf))
          return false;
      }
      else
      {
        if(!BEncodeWriteDictEntry("N", N, buf))
          return false;
        // r5n counter
        if(!BEncodeWriteDictInt("R", R, buf))
          return false;
      }
      // txid
      if(!BEncodeWriteDictInt("T", T, buf))
        return false;
      // protocol version
      if(!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    FindIntroMessage::HandleMessage(
        llarp_dht_context* ctx,
        std::vector< llarp::dht::IMessage* >& replies) const
    {
      if(R > 5)
      {
        llarp::LogError("R value too big, ", R, "> 5");
        return false;
      }
      auto& dht = ctx->impl;
      Key_t peer;
      std::set< Key_t > exclude = {dht.OurKey(), From};
      if(N.IsZero())
      {
        const auto introset = dht.GetIntroSetByServiceAddress(S);
        if(introset)
        {
          service::IntroSet i = *introset;
          replies.push_back(new GotIntroMessage({i}, T));
        }
        else
        {
          if(iterative)
          {
            // we are iterative and don't have it, reply with a direct reply
            replies.push_back(new GotIntroMessage({}, T));
          }
          else
          {
            // we are recursive
            if(dht.nodes->FindCloseExcluding(S, peer, exclude))
            {
              if(relayed)
                dht.LookupIntroSetForPath(S, T, pathID, peer);
              else if((peer ^ dht.OurKey())
                      > (peer
                         ^ From))  // peer is closer than us, recursive search
                dht.LookupIntroSet(S, From, T, peer);
              else  // we are closer than peer so do iterative search
                dht.LookupIntroSet(S, From, T, peer, true);
            }
            else
            {
              llarp::LogError(
                  "cannot find closer peers for introset lookup for ", S);
            }
          }
        }
      }
      else
      {
        if(relayed)
        {
          // tag lookup
          if(dht.nodes->GetRandomNodeExcluding(peer, exclude))
          {
            dht.LookupTagForPath(N, T, pathID, peer);
          }
          else
          {
            llarp::LogWarn("no closer peers for tag ", N.ToString());
          }
        }
        else
        {
          auto introsets = dht.FindRandomIntroSetsWithTag(N);
          if(iterative || R == 0)
          {
            std::vector< service::IntroSet > reply;
            for(const auto& introset : introsets)
            {
              reply.push_back(introset);
            }
            // we are iterative and don't have it, reply with a direct reply
            replies.push_back(new GotIntroMessage(reply, T));
          }
          else
          {
            // tag lookup
            if(dht.nodes->GetRandomNodeExcluding(peer, exclude))
            {
              dht.LookupTag(N, From, T, peer, introsets, R - 1);
            }
          }
        }
      }
      return true;
    }
  }  // namespace dht
}  // namespace llarp