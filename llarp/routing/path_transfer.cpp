#include <llarp/messages/path_transfer.hpp>
#include "../buffer.hpp"
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
      if(!BEncodeMaybeReadDictInt("S", S, read, key, val))
        return false;
      if(llarp_buffer_eq(key, "T"))
      {
        if(T)
          delete T;
        T = new service::ProtocolFrame();
        return T->BDecode(val);
      }
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

      if(!BEncodeWriteDictInt("S", S, buf))
        return false;

      if(!bencode_write_bytestring(buf, "T", 1))
        return false;
      if(!T->BEncode(buf))
        return false;

      if(!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
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
      if(!path)
      {
        llarp::LogWarn("No such path for path transfer pathid=", P);
        return false;
      }
      if(!T)
      {
        llarp::LogError("no data to transfer on data message");
        return false;
      }

      byte_t tmp[service::MAX_PROTOCOL_MESSAGE_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!T->BEncode(&buf))
      {
        llarp::LogWarn("failed to transfer data message, encode failed");
        return false;
      }
      // rewind
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      // send
      llarp::LogInfo("Transfer ", buf.sz, " bytes", " to ", P);
      return path->HandleDownstream(buf, Y, r);
    }

  }  // namespace routing
}  // namespace llarp
