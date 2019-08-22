#ifndef LLARP_EV_PIPE_HPP
#define LLARP_EV_PIPE_HPP

#include <ev/ev.hpp>

/// a unidirectional packet pipe
struct llarp_ev_pkt_pipe : public llarp::ev_io
{
  llarp_ev_pkt_pipe(llarp_ev_loop_ptr loop);

  /// start the pipe, initialize fds
  bool
  StartPipe();

  /// write to the pipe from outside the event loop
  /// returns true on success
  /// returns false on failure
  bool
  Write(const llarp_buffer_t& buf);

  /// override me to handle a packet from the other side in the owned event loop
  virtual void
  OnRead(const llarp_buffer_t& buf) = 0;

  ssize_t
  do_write(void* buf, size_t sz) override;

  bool
  tick() override;

  int
  read(byte_t* buf, size_t sz) override;

 private:
  llarp_ev_loop_ptr m_Loop;
  int writefd;
};

#endif
