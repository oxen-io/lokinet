#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <llarp/bencode.h>
#include <llarp/link.h>

#include <queue>
#include <vector>

namespace llarp
{
  typedef std::vector< byte_t > OutboundMessage;

  struct InboundMessageHandler
  {
    InboundMessageHandler(llarp_router* router);
    dict_reader reader;

    static bool
    OnKey(dict_reader* r, llarp_buffer_t* buf);

    bool
    ProcessFrom(llarp_link_session* from, llarp_buffer_t buf);

    bool
    FlushReplies();

   private:
    char msgtype;
    bool firstkey;
    uint64_t proto;
    llarp_router* router;
    llarp_link_session* from;
    std::queue< OutboundMessage > sendq;
  };
}

#endif
