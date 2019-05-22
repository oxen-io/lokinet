#include <routing/message_parser.hpp>

#include <messages/dht.hpp>
#include <messages/discard.hpp>
#include <messages/exit.hpp>
#include <messages/path_confirm.hpp>
#include <messages/path_latency.hpp>
#include <messages/path_transfer.hpp>
#include <messages/transfer_traffic.hpp>
#include <path/path_types.hpp>
#include <util/mem.hpp>

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

    InboundMessageParser::InboundMessageParser()
        : firstKey(false)
        , ourKey('\0')
        , msg(nullptr)
        , m_Holder(std::make_unique< MessageHolder >())
    {
    }

    InboundMessageParser::~InboundMessageParser()
    {
    }

    bool
    InboundMessageParser::operator()(llarp_buffer_t* buffer,
                                     llarp_buffer_t* key)
    {
      if(key == nullptr && firstKey)
      {
        // empty dict
        return false;
      }
      if(!key)
        return true;
      if(firstKey)
      {
        llarp_buffer_t strbuf;
        if(!(*key == "A"))
          return false;
        if(!bencode_read_string(buffer, &strbuf))
          return false;
        if(strbuf.sz != 1)
          return false;
        ourKey = *strbuf.cur;
        LogDebug("routing message '", key, "'");
        switch(ourKey)
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
        firstKey = false;
        return msg != nullptr;
      }
      else
      {
        return msg->DecodeKey(*key, buffer);
      }
    }

    bool
    InboundMessageParser::ParseMessageBuffer(const llarp_buffer_t& buf,
                                             IMessageHandler* h,
                                             const PathID_t& from,
                                             AbstractRouter* r)
    {
      bool result = false;
      msg         = nullptr;
      firstKey    = true;
      ManagedBuffer copiedBuf(buf);
      auto& copy = copiedBuf.underlying;
      if(bencode_read_dict(*this, &copy))
      {
        msg->from = from;
        result    = msg->HandleMessage(h, r);
        if(!result)
        {
          llarp::LogWarn("Failed to handle inbound routing message ", ourKey);
        }
      }
      else
      {
        llarp::LogError("read dict failed in routing layer");
        llarp::DumpBuffer< llarp_buffer_t, 128 >(buf);
      }
      if(msg)
        msg->Clear();
      msg = nullptr;
      return result;
    }
  }  // namespace routing
}  // namespace llarp
