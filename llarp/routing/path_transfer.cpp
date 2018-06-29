#include <llarp/messages/path_transfer.hpp>
#include "../router.hpp"

namespace llarp
{
  namespace routing
  {
    PathTransferMessage::PathTransferMessage() : IMessage()
    {
    }

    PathTransferMessage::~PathTransferMessage()
    {
    }

    bool
    PathTransferMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("P", P, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("T", T, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("V", V, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("Y", Y, read, key, val))
        return false;
      return false;
    }

    bool
    PathTransferMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "T"))
        return false;
      if(!BEncodeWriteDictEntry("P", P, buf))
        return false;
      if(!BEncodeWriteDictEntry("T", T, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "V", LLARP_PROTO_VERSION))
        return false;
      if(!BEncodeWriteDictEntry("Y", Y, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    PathTransferMessage::HandleMessage(IMessageHandler* h,
                                       llarp_router* r) const
    {
      auto path = r->paths.GetByUpstream(r->pubkey(), P);
      if(path)
      {
        return path->HandleDownstream(T.Buffer(), Y, r);
      }
      llarp::Warn("No such local path for path transfer src=", from,
                  " dst=", P);
      return false;
    }
  }  // namespace routing

}  // namespace llarp