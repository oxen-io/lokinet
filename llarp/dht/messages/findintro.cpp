#include <dht/context.hpp>
#include <dht/messages/findintro.hpp>
#include <dht/messages/gotintro.hpp>
#include <routing/message.hpp>
#include <router/abstractrouter.hpp>
#include <nodedb.hpp>

namespace llarp
{
  namespace dht
  {
    FindIntroMessage::~FindIntroMessage() = default;

    bool
    FindIntroMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* val)
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

      if(!BEncodeMaybeVerifyVersion("V", version, LLARP_PROTO_VERSION, read, k,
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
      if(N.Empty())
      {
        // recursion
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
        // recursion
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
        llarp_dht_context* ctx, std::vector< IMessage::Ptr_t >& replies) const
    {
      if(R > 5)
      {
        llarp::LogError("R value too big, ", R, "> 5");
        return false;
      }
      auto& dht = *ctx->impl;
      if(dht.pendingIntrosetLookups().HasPendingLookupFrom(TXOwner{From, T}))
      {
        llarp::LogWarn("duplicate FIM from ", From, " txid=", T);
        return false;
      }
      Key_t peer;
      std::set< Key_t > exclude = {dht.OurKey(), From};
      if(N.Empty())
      {
        llarp::LogInfo("lookup ", S.ToString());
        const auto introset = dht.GetIntroSetByServiceAddress(S);
        if(introset)
        {
          service::IntroSet i = *introset;
          replies.emplace_back(new GotIntroMessage({i}, T));
          return true;
        }

        if(R == 0)
        {
          // we don't have it
          Key_t target = S.ToKey();
          Key_t closer;
          // find closer peer
          if(!dht.Nodes()->FindClosest(target, closer))
            return false;
          if(relayed)
            dht.LookupIntroSetForPath(S, T, pathID, closer);
          else
            replies.emplace_back(new GotIntroMessage(From, closer, T));
          return true;
        }

        Key_t us     = dht.OurKey();
        Key_t target = S.ToKey();
        // we are recursive
        // use nodedb for closest router
        const auto rc = dht.GetRouter()->nodedb()->FindClosestToAddress(target);
        Key_t peer(rc.pubkey.data());
        if(peer == us)
        {
          if(not dht.Nodes()->FindCloseExcluding(target, peer, exclude))
          {
            // we can not find second closest router
            if(relayed)
              return false;
            replies.emplace_back(new GotIntroMessage({}, T));
            return true;
          }
        }
        if(relayed)
          dht.LookupIntroSetForPath(S, T, pathID, peer);
        else
        {
          if((us ^ target) < (peer ^ target))
          {
            // we are not closer than our peer to the target so don't
            // recurse farther
            replies.emplace_back(new GotIntroMessage({}, T));
            return true;
          }
          if(R > 0)
            dht.LookupIntroSetRecursive(S, From, T, peer, R - 1);
          else
            dht.LookupIntroSetIterative(S, From, T, peer);
        }
        return true;
      }

      if(relayed)
      {
        // tag lookup
        if(dht.Nodes()->GetRandomNodeExcluding(peer, exclude))
        {
          dht.LookupTagForPath(N, T, pathID, peer);
        }
        else
        {
          // no more closer peers
          replies.emplace_back(new GotIntroMessage({}, T));
          return true;
        }
      }
      else
      {
        if(R == 0)
        {
          // base case
          auto introsets = dht.FindRandomIntroSetsWithTagExcluding(N, 2, {});
          std::vector< service::IntroSet > reply;
          for(const auto& introset : introsets)
          {
            reply.push_back(introset);
          }
          replies.emplace_back(new GotIntroMessage(reply, T));
          return true;
        }
        if(R < 5)
        {
          // tag lookup
          if(dht.Nodes()->GetRandomNodeExcluding(peer, exclude))
          {
            dht.LookupTagRecursive(N, From, T, peer, R - 1);
          }
          else
          {
            replies.emplace_back(new GotIntroMessage({}, T));
          }
        }
        else
        {
          // too big R value
          replies.emplace_back(new GotIntroMessage({}, T));
        }
      }

      return true;
    }
  }  // namespace dht
}  // namespace llarp
