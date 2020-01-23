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
    /// 2 ** 12 which is 4096 nodes, after which this starts to fail "more"
    const uint64_t FindIntroMessage::MaxRecursionDepth = 12;
    FindIntroMessage::~FindIntroMessage()              = default;

    bool
    FindIntroMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* val)
    {
      bool read = false;

      if(!BEncodeMaybeReadDictEntry("N", tagName, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictInt("R", recursionDepth, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictEntry("S", serviceAddress, read, k, val))
        return false;

      if(!BEncodeMaybeReadDictInt("T", txID, read, k, val))
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
      if(tagName.Empty())
      {
        // recursion
        if(!BEncodeWriteDictInt("R", recursionDepth, buf))
          return false;
        // service address
        if(!BEncodeWriteDictEntry("S", serviceAddress, buf))
          return false;
      }
      else
      {
        if(!BEncodeWriteDictEntry("N", tagName, buf))
          return false;
        // recursion
        if(!BEncodeWriteDictInt("R", recursionDepth, buf))
          return false;
      }
      // txid
      if(!BEncodeWriteDictInt("T", txID, buf))
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
      if(recursionDepth > MaxRecursionDepth)
      {
        llarp::LogError("recursion depth big, ", recursionDepth, "> ",
                        MaxRecursionDepth);
        return false;
      }
      auto& dht = *ctx->impl;
      if(dht.pendingIntrosetLookups().HasPendingLookupFrom(TXOwner{From, txID}))
      {
        llarp::LogWarn("duplicate FIM from ", From, " txid=", txID);
        return false;
      }
      Key_t peer;
      std::set< Key_t > exclude = {dht.OurKey(), From};
      if(tagName.Empty())
      {
        if(serviceAddress.IsZero())
        {
          // we dont got it
          replies.emplace_back(new GotIntroMessage({}, txID));
        }
        llarp::LogInfo("lookup ", serviceAddress.ToString());
        const auto introset = dht.GetIntroSetByServiceAddress(serviceAddress);
        if(introset)
        {
          replies.emplace_back(new GotIntroMessage({*introset}, txID));
          return true;
        }

        const Key_t target = serviceAddress.ToKey();
        const Key_t us     = dht.OurKey();

        if(recursionDepth == 0)
        {
          // we don't have it

          Key_t closer;
          // find closer peer
          if(!dht.Nodes()->FindClosest(target, closer))
            return false;
          replies.emplace_back(new GotIntroMessage(From, closer, txID));
          return true;
        }

        // we are recursive
        const auto rc = dht.GetRouter()->nodedb()->FindClosestTo(target);

        peer = Key_t(rc.pubkey);

        if((us ^ target) < (peer ^ target) || peer == us)
        {
          // we are not closer than our peer to the target so don't
          // recurse farther
          replies.emplace_back(new GotIntroMessage({}, txID));
          return true;
        }
        if(relayed)
        {
          dht.LookupIntroSetForPath(serviceAddress, txID, pathID, peer,
                                    recursionDepth - 1);
        }
        else
        {
          dht.LookupIntroSetRecursive(serviceAddress, From, txID, peer,
                                      recursionDepth - 1);
        }
        return true;
      }

      if(relayed)
      {
        // tag lookup
        if(dht.Nodes()->GetRandomNodeExcluding(peer, exclude))
        {
          dht.LookupTagForPath(tagName, txID, pathID, peer);
        }
        else
        {
          // no more closer peers
          replies.emplace_back(new GotIntroMessage({}, txID));
          return true;
        }
      }
      else
      {
        if(recursionDepth == 0)
        {
          // base case
          auto introsets =
              dht.FindRandomIntroSetsWithTagExcluding(tagName, 2, {});
          std::vector< service::IntroSet > reply;
          for(const auto& introset : introsets)
          {
            reply.emplace_back(introset);
          }
          replies.emplace_back(new GotIntroMessage(reply, txID));
          return true;
        }
        if(recursionDepth < MaxRecursionDepth)
        {
          // tag lookup
          if(dht.Nodes()->GetRandomNodeExcluding(peer, exclude))
          {
            dht.LookupTagRecursive(tagName, From, txID, peer,
                                   recursionDepth - 1);
          }
          else
          {
            replies.emplace_back(new GotIntroMessage({}, txID));
          }
        }
        else
        {
          // too big recursion depth
          replies.emplace_back(new GotIntroMessage({}, txID));
        }
      }

      return true;
    }
  }  // namespace dht
}  // namespace llarp
