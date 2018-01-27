#ifndef LLARP_MSG_HANDLER_H_
#define LLARP_MSG_HANDLER_H_
#include <llarp/buffer.h>
#include <llarp/link.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct llarp_frame_handler
  {
    struct llarp_obmd * outbound;
    struct llarp_ibmq * inbound;
    bool (*process)(struct llarp_frame_handler *, struct llarp_link_session *, llarp_buffer_t);
  };

  struct llarp_msg_handler
  {
    struct llarp_path_context * paths;
  };
  
  struct llarp_msg_muxer
  {
    /** get a message handler for a link level message given msg.a */
    struct llarp_frame_handler * (*link_handler_for)(const char *);
    /** get a message handler for a routing layer message given msg.A */
    struct llarp_msg_handler * (*routing_handler_for)(const char *);
  };

  /** fill function pointers with default values */
  void llarp_msg_handler_mux_init(struct llarp_msg_muxer * muxer);

#ifdef __cplusplus
}
#endif

#endif
