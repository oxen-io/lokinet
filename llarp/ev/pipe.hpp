#ifndef LLARP_EV_PIPE_HPP
#define LLARP_EV_PIPE_HPP

#include <ev/ev.hpp>

struct llarp_ev_pipe
{
  llarp_ev_pipe(llarp_ev_loop_ptr loop);

 private:
  std::pair< int, int > m_EventFDs;
  std::pair< int, int > m_ExternFDs;
};

#endif
