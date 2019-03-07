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
        , key('\0')
        , msg(nullptr)
        , m_Holder(std::make_unique< MessageHolder >())
    {
      reader.user   = this;
      reader.on_key = &OnKey;
    }

    InboundMessageParser::~InboundMessageParser()
    {
    }

    bool
    InboundMessageParser::OnKey(dict_reader* r, llarp_buffer_t* key)
    {
      InboundMessageParser* self =
          static_cast< InboundMessageParser* >(r->user);

      if(key == nullptr && self->firstKey)
      {
        // empty dict
        return false;
      }
      if(!key)
        return true;
      if(self->firstKey)
      {
        llarp_buffer_t strbuf;
        if(!(*key == "A"))
          return false;
        if(!bencode_read_string(r->buffer, &strbuf))
          return false;
        if(strbuf.sz != 1)
          return false;
        self->key = *strbuf.cur;
        LogDebug("routing message '", self->key, "'");
        switch(self->key)
        {
          case 'D':
            self->msg = &self->m_Holder->D;
            break;
          case 'L':
            self->msg = &self->m_Holder->L;
            break;
          case 'M':
            self->msg = &self->m_Holder->M;
            break;
          case 'P':
            self->msg = &self->m_Holder->P;
            break;
          case 'T':
            self->msg = &self->m_Holder->T;
            break;
          case 'H':
            self->msg = &self->m_Holder->H;
            break;
          case 'I':
            self->msg = &self->m_Holder->I;
            break;
          case 'G':
            self->msg = &self->m_Holder->G;
            break;
          case 'J':
            self->msg = &self->m_Holder->J;
            break;
          case 'O':
            self->msg = &self->m_Holder->O;
            break;
          case 'U':
            self->msg = &self->m_Holder->U;
            break;
          case 'C':
            self->msg = &self->m_Holder->C;
            break;
          default:
            llarp::LogError("invalid routing message id: ", *strbuf.cur);
        }
        self->firstKey = false;
        return self->msg != nullptr;
      }
      else
      {
        return self->msg->DecodeKey(*key, r->buffer);
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
      if(bencode_read_dict(&copy, &reader))
      {
        msg->from = from;
        result    = msg->HandleMessage(h, r);
        if(!result)
          llarp::LogWarn("Failed to handle inbound routing message ", key);
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
