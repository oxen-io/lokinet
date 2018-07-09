#pragma once

#include "llarp/buffer.h"

#include <queue>

struct sendbuf_t
{
  sendbuf_t(size_t s) : sz(s)
  {
    buf = new byte_t[s];
  }

  ~sendbuf_t()
  {
    delete[] buf;
  }

  byte_t *buf;
  size_t sz;

  size_t
  size() const
  {
    return sz;
  }

  byte_t *
  data()
  {
    return buf;
  }
};

typedef std::queue< sendbuf_t * > sendqueue_t;