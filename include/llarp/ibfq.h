#ifndef LLARP_IBFQ_H_
#define LLARP_IBFQ_H_
#include <llarp/buffer.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  // forward declare
  struct llarp_msg_muxer;
  
  struct llarp_link_queue;
  
  struct llarp_link_queue * llarp_init_link_queue();
  void llarp_free_link_queue(struct llarp_link_queue ** queue);
  /** 
      offer a full frame to the inbound frame queue 
      return true if successfully added
      return false if the queue is full
   */
  bool llarp_link_offer_frame(struct llarp_link_queue * queue, llarp_buffer_t msg);
  /** return true if we have more messages to process */
  bool llarp_link_queue_process(struct llarp_link_queue * queue, struct llarp_msg_muxer * muxer);

#ifdef __cplusplus
}
#endif

#endif
