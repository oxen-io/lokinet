#include <ev/pipe.hpp>
#include <utility>

#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <fcntl.h>

llarp_ev_pkt_pipe::llarp_ev_pkt_pipe(llarp_ev_loop_ptr loop)
    : llarp::ev_io(-1, new LosslessWriteQueue_t()), m_Loop(std::move(loop))
{
}

bool
llarp_ev_pkt_pipe::StartPipe()
{
#if defined(_WIN32)
  llarp::LogError("llarp_ev_pkt_pipe not supported on win32");
  return false;
#else
  int _fds[2];
  if (pipe(_fds) == -1 && fcntl(_fds[0], F_SETFL, fcntl(_fds[0], F_GETFL) | O_NONBLOCK))
  {
    return false;
  }
  fd = _fds[0];
  writefd = _fds[1];
  return m_Loop->add_pipe(this);
#endif
}

int
llarp_ev_pkt_pipe::read(byte_t* pkt, size_t sz)
{
  auto res = ::read(fd, pkt, sz);
  if (res <= 0)
    return res;
  llarp::LogDebug("read ", res, " on pipe");
  llarp_buffer_t buf(pkt, res);
  OnRead(buf);
  return res;
}

ssize_t
llarp_ev_pkt_pipe::do_write(void* buf, size_t sz)
{
  return ::write(writefd, buf, sz);
}

bool
llarp_ev_pkt_pipe::Write(const llarp_buffer_t& pkt)
{
  const ssize_t sz = pkt.sz;
  if (do_write(pkt.base, pkt.sz) != sz)
  {
    llarp::LogDebug("queue write ", pkt.sz);
    return queue_write(pkt.base, pkt.sz);
  }
  return true;
}

bool
llarp_ev_pkt_pipe::tick()
{
  llarp::ev_io::flush_write();
  return true;
}
