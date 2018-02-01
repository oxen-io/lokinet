#ifndef LLARP_MSG_HANDLER_H_
#define LLARP_MSG_HANDLER_H_
#include <llarp/buffer.h>
#include <llarp/dht.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* foward declare */
struct llarp_msg_muxer;
struct llarp_link_session;
struct llarp_router;

struct llarp_frame_handler {
  /**
   * participating paths
   */
  struct llarp_path_context *paths;

  /**
   * parent muxer
   */
  struct llarp_msg_muxer *parent;

  /**
     handle fully formed frame from link session
   */
  bool (*process)(struct llarp_frame_handler *, struct llarp_link_session *,
                  llarp_buffer_t);
};

struct llarp_msg_handler {
  struct llarp_path_context *paths;
  struct llarp_dht_context *dht;
  bool (*process)(struct llarp_msg_handler *, llarp_buffer_t);
};

struct llarp_msg_muxer {
  /** get a message handler for a link level message given msg.a */
  struct llarp_frame_handler *(*link_handler_for)(struct llarp_router *,
                                                  const char);
  /** get a message handler for a routing layer message given msg.A */
  struct llarp_msg_handler *(*routing_handler_for)(struct llarp_router *,
                                                   const char);
};

/** fill function pointers with default values */
void llarp_msg_muxer_init(struct llarp_msg_muxer *muxer);

#ifdef __cplusplus
}
#endif

#endif
