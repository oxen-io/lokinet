#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <llarp/bencode.h>
#include <llarp/link.h>
#include <llarp/relay_commit.hpp>

#include <queue>
#include <vector>

namespace llarp
{
  typedef std::vector< byte_t > Message;

  struct InboundMessageHandler
  {
    InboundMessageHandler(llarp_router* router);
    dict_reader reader;

    static bool
    OnKey(dict_reader* r, llarp_buffer_t* buf);

    /// start processig message from a link session
    bool
    ProcessFrom(llarp_link_session* from, llarp_buffer_t buf);

    /// called when the message is fully read
    /// return true when the message was accepted otherwise returns false
    bool
    MessageDone();

    /// called to send any replies
    bool
    FlushReplies();

   private:
    bool
    DecodeLIM(llarp_buffer_t key, llarp_buffer_t* buff);

    bool
    DecodeDHT(llarp_buffer_t key, llarp_buffer_t* buff);

    bool
    DecodeLRCM(llarp_buffer_t key, llarp_buffer_t* buff);

   private:
    char msgtype;
    bool firstkey;
    uint64_t proto;
    llarp_router* router;
    llarp_link_session* from;
    llarp::LR_CommitMessage lrcm;
    std::queue< Message > sendq;
  };
}

#endif
