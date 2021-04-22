#include "message_parser.hpp"

#include <llarp/exit/exit_messages.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/path/path_types.hpp>
#include "dht_message.hpp"
#include "path_confirm_message.hpp"
#include "path_latency_message.hpp"
#include "path_transfer_message.hpp"
#include "transfer_traffic_message.hpp"
#include <llarp/util/mem.hpp>

namespace llarp
{
  namespace routing
  {
    struct InboundMessageParser::MessageHolder
    {
      DataDiscardMessage D;
      PathLatencyMessage L;
      DHTMessage M;
      PathConfirmMessage P;
      PathTransferMessage T;
      service::ProtocolFrame H;
      TransferTrafficMessage I;
      GrantExitMessage G;
      RejectExitMessage J;
      ObtainExitMessage O;
      UpdateExitMessage U;
      CloseExitMessage C;
    };

    InboundMessageParser::InboundMessageParser() : m_Holder(std::make_unique<MessageHolder>())
    {}

    InboundMessageParser::~InboundMessageParser() = default;

    bool
    InboundMessageParser::operator()(llarp_buffer_t* buffer, llarp_buffer_t* key)
    {
      if (key == nullptr && firstKey)
      {
        // empty dict
        return false;
      }
      if (!key)
        return true;
      if (firstKey)
      {
        llarp_buffer_t strbuf;
        if (!(*key == "A"))
          return false;
        if (!bencode_read_string(buffer, &strbuf))
          return false;
        if (strbuf.sz != 1)
          return false;
        ourKey = *strbuf.cur;
        LogDebug("routing message '", std::string{ourKey, 1}, "'");
        switch (ourKey)
        {
          case 'D':
            msg = &m_Holder->D;
            break;
          case 'L':
            msg = &m_Holder->L;
            break;
          case 'M':
            msg = &m_Holder->M;
            break;
          case 'P':
            msg = &m_Holder->P;
            break;
          case 'T':
            msg = &m_Holder->T;
            break;
          case 'H':
            msg = &m_Holder->H;
            break;
          case 'I':
            msg = &m_Holder->I;
            break;
          case 'G':
            msg = &m_Holder->G;
            break;
          case 'J':
            msg = &m_Holder->J;
            break;
          case 'O':
            msg = &m_Holder->O;
            break;
          case 'U':
            msg = &m_Holder->U;
            break;
          case 'C':
            msg = &m_Holder->C;
            break;
          default:
            llarp::LogError("invalid routing message id: ", *strbuf.cur);
        }
        if (msg)
          msg->version = version;
        firstKey = false;
        return msg != nullptr;
      }

      return msg->DecodeKey(*key, buffer);
    }

    bool
    InboundMessageParser::ParseMessageBuffer(
        const llarp_buffer_t& buf, IMessageHandler* h, const PathID_t& from, AbstractRouter* r)
    {
      bool result = false;
      msg = nullptr;
      firstKey = true;
      ManagedBuffer copiedBuf(buf);
      auto& copy = copiedBuf.underlying;
      uint64_t v = 0;
      if (BEncodeSeekDictVersion(v, &copy, 'V'))
      {
        version = v;
      }
      if (bencode_read_dict(*this, &copy))
      {
        msg->from = from;
        LogDebug("handle routing message ", msg->S, " from ", from);
        result = msg->HandleMessage(h, r);
        if (!result)
        {
          llarp::LogWarn("Failed to handle inbound routing message ", ourKey);
        }
      }
      else
      {
        llarp::LogError("read dict failed in routing layer");
        llarp::DumpBuffer<llarp_buffer_t, 128>(buf);
      }
      if (msg)
        msg->Clear();
      msg = nullptr;
      version = 0;
      return result;
    }
  }  // namespace routing
}  // namespace llarp
