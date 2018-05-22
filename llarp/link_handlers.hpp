#ifndef LLARP_LINK_HANDLERS_HPP
#define LLARP_LINK_HANDLERS_HPP
#include <llarp/msg_handler.h>

namespace llarp
{
  namespace frame
  {
    bool
    process_intro(struct llarp_frame_handler* h, struct llarp_link_session* s,
                  llarp_buffer_t msg);
    bool
    process_relay_commit(struct llarp_frame_handler* h,
                         struct llarp_link_session* s, llarp_buffer_t msg);
    bool
    process_relay_down(struct llarp_frame_handler* h,
                       struct llarp_link_session* s, llarp_buffer_t msg);
    bool
    process_relay_up(struct llarp_frame_handler* h,
                     struct llarp_link_session* s, llarp_buffer_t msg);
    bool
    process_relay_accept(struct llarp_frame_handler* h,
                         struct llarp_link_session* s, llarp_buffer_t msg);
    bool
    process_relay_status(struct llarp_frame_handler* h,
                         struct llarp_link_session* s, llarp_buffer_t msg);
    bool
    process_relay_exit(struct llarp_frame_handler* h,
                       struct llarp_link_session* s, llarp_buffer_t msg);
  }  // namespace frame
}  // namespace llarp

#endif
