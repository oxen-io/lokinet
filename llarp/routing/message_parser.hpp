#ifndef LLARP_ROUTING_MESSAGE_PARSER_HPP
#define LLARP_ROUTING_MESSAGE_PARSER_HPP

#include <messages/dht.hpp>
#include <messages/discard.hpp>
#include <messages/path_confirm.hpp>
#include <messages/path_latency.hpp>
#include <messages/path_transfer.hpp>
#include <path/path_types.hpp>
#include <util/bencode.hpp>
#include <util/buffer.h>

namespace llarp
{
  struct Router;

  namespace routing
  {
    struct IMessageHandler;

    struct InboundMessageParser
    {
      InboundMessageParser();

      bool
      ParseMessageBuffer(llarp_buffer_t buf, IMessageHandler* handler,
                         const PathID_t& from, llarp::Router* r);

     private:
      static bool
      OnKey(dict_reader* r, llarp_buffer_t* key);
      bool firstKey;
      char key;
      dict_reader reader;

      struct MessageHolder
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

      IMessage * msg = nullptr;
      MessageHolder m_Holder;
    };
  }
}
#endif
