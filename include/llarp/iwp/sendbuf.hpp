#pragma once

#include <llarp/buffer.h>
#include <llarp/time.h>
#include <memory>
#include <queue>

struct sendbuf_t
{
  sendbuf_t()
  {
    _sz = 0;
  }

  sendbuf_t(const sendbuf_t &other)
  {
    _sz = std::min(other._sz, sizeof(_buf));
    memcpy(_buf, other._buf, _sz);
  }

  sendbuf_t(size_t s)
  {
    if(s > sizeof(_buf))
      throw std::logic_error("sendbuf too big");
    _sz = s;
  }

  ~sendbuf_t()
  {
  }

  size_t
  size() const
  {
    return _sz;
  }

  byte_t *
  data()
  {
    return _buf;
  }

  llarp_buffer_t
  Buffer()
  {
    llarp_buffer_t buf;
    buf.base = _buf;
    buf.sz   = _sz;
    buf.cur  = buf.base;
    return buf;
  }

  struct GetTime
  {
    llarp_time_t
    operator()(const sendbuf_t &buf) const
    {
      return buf.timestamp;
    }
  };

  struct PutTime
  {
    void
    operator()(sendbuf_t &buf) const
    {
      buf.timestamp = llarp_time_now_ms();
    }
  };

  struct Compare
  {
    bool
    operator()(const sendbuf_t &left, const sendbuf_t &right) const
    {
      return left.timestamp > right.timestamp;
    }
  };

  llarp_time_t timestamp = 0;

 private:
  size_t _sz;
  byte_t _buf[1500];
};
