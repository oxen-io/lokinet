#include <llarp/dht/context.hpp>
#include <llarp/dht/messages/findintro.hpp>
#include <llarp/dht/messages/gotintro.hpp>

namespace llarp
{
  namespace dht
  {
    FindIntroMessage::~FindIntroMessage()
    {
    }

    bool
    FindIntroMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* val)
    {
      // TODO: implement me
      return false;
    }

    bool
    FindIntroMessage::BEncode(llarp_buffer_t* buf) const
    {
      /// TODO: implement me
      return false;
    }

    bool
    FindIntroMessage::HandleMessage(
        llarp_dht_context* ctx,
        std::vector< llarp::dht::IMessage* >& replies) const
    {
      auto& dht     = ctx->impl;
      auto introset = dht.GetIntroSetByServiceAddress(S);
      if(introset)
      {
        replies.push_back(new GotIntroMessage(T, introset));
        return true;
      }
      else
      {
        // do lookup
      }
      return false;
    }

  }  // namespace dht
}  // namespace llarp