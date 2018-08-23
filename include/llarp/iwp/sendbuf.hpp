#pragma once

#include <llarp/buffer.h>
#include <llarp/time.h>
#include <memory>
#include <queue>

struct sendbuf_t
{
  sendbuf_t(size_t s) : sz(s)
  {
    _buf = new byte_t[s];
  }

  ~sendbuf_t()
  {
    if(_buf)
      delete[] _buf;
  }

  size_t sz;

  byte_t priority = 255;

  size_t
  size() const
  {
    return sz;
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
    buf.sz   = sz;
    buf.cur  = buf.base;
    return buf;
  }

  struct GetTime
  {
    llarp_time_t
    operator()(const sendbuf_t *buf) const
    {
      return buf->timestamp;
    }
  };

  struct PutTime
  {
    void
    operator()(sendbuf_t *buf) const
    {
      buf->timestamp = llarp_time_now_ms();
    }
  };

  struct Compare
  {
    bool
    operator()(const sendbuf_t *left, const sendbuf_t *right) const
    {
      return left->timestamp < right->timestamp;
    }
  };

  llarp_time_t timestamp = 0;

 private:
  byte_t *_buf = nullptr;
};
