#pragma once

#include "llarp/buffer.h"

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

 private:
  byte_t *_buf = nullptr;
};

typedef std::queue< sendbuf_t * > sendqueue_t;