#pragma once

#include "buffer.hpp"
#include "llarp/time.h"
#include "llarp/types.h"

#include <vector>

struct InboundMessage
{
  uint64_t msgid;
  byte_t *_buf;
  size_t sz;
  llarp_time_t queued = 0;

  InboundMessage(uint64_t id, const std::vector< byte_t > &m) : msgid(id)
  {
    sz = m.size();
    if(sz)
    {
      _buf = new byte_t[sz];
      memcpy(_buf, m.data(), sz);
    }
    else
      _buf = nullptr;
  }

  ~InboundMessage()
  {
    if(_buf)
      delete[] _buf;
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
    return llarp::InitBuffer(_buf, sz);
  }

  struct GetTime
  {
    llarp_time_t
    operator()(const InboundMessage *msg)
    {
      return msg->queued;
    }
  };

  struct OrderCompare
  {
    bool
    operator()(const InboundMessage *left, const InboundMessage *right) const
    {
      return left->msgid < right->msgid;
    }
  };

  struct PutTime
  {
    void
    operator()(InboundMessage *msg)
    {
      msg->queued = llarp_time_now_ms();
    }
  };
};
