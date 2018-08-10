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
      bool read = false;

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
      if((!relayed) && dht.FindPendingTX(From, T))
      {
        llarp::LogWarn("duplicate FIM from ", From, " txid=", T);
        return false;
      }
      Key_t peer;
      std::set< Key_t > exclude = {dht.OurKey(), From};
      if(N.ToString().empty())
      {
        const auto introset = dht.GetIntroSetByServiceAddress(S);
        if(introset)
        {
          llarp::LogInfo("introset found locally");
          service::IntroSet i = *introset;
          replies.push_back(new GotIntroMessage({i}, T));
        }
        else
        {
          if(R == 0 && !relayed)
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
              else if(R >= 1)
                dht.LookupIntroSet(S, From, T, peer, R - 1);
              else
                dht.LookupIntroSet(S, From, T, peer, 0);
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
          if(R == 0)
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