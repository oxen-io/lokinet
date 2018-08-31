#pragma once

#include "buffer.hpp"
#include "llarp/time.h"
#include "llarp/types.h"

#include <vector>

struct InboundMessage
{
  uint64_t msgid;
  std::vector< byte_t > msg;
  llarp_time_t queued = 0;

  InboundMessage()
  {
  }

  InboundMessage(uint64_t id, const std::vector< byte_t > &m)
      : msgid(id), msg(m)
  {
  }

  ~InboundMessage()
  {
  }

  bool
  operator<(const InboundMessage &other) const
  {
    // order in ascending order for codel queue
    return msgid < other.msgid;
  }

  llarp_buffer_t
  Buffer() const
  {
    return llarp::ConstBuffer(msg);
  }

  struct GetTime
  {
    llarp_time_t
    operator()(const InboundMessage &msg)
    {
      return msg.queued;
    }
  };

  struct OrderCompare
  {
    bool
    operator()(const InboundMessage &left, const InboundMessage &right) const
    {
      return left < right;
    }
  };

  struct PutTime
  {
    void
    operator()(InboundMessage &msg)
    {
      msg.queued = llarp_time_now_ms();
    }
  };
};
