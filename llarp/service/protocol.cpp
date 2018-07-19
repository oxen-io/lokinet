#include <llarp/service/protocol.hpp>

namespace llarp
{
  namespace service
  {
    ProtocolMessage::ProtocolMessage(ProtocolType t) : proto(t)
    {
    }

    ProtocolMessage::~ProtocolMessage()
    {
    }

    bool
    ProtocolMessage::BEncode(llarp_buffer_t* buf) const
    {
      // TODO: implement me
      return false;
    }

    bool
    ProtocolMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      // TODO: implement me
      return false;
    }

    void
    ProtocolMessage::PutBuffer(llarp_buffer_t buf)
    {
      payload.resize(buf.sz);
      memcpy(payload.data(), buf.base, buf.sz);
    }
  }  // namespace service
}  // namespace llarp