#ifndef LLARP_IBMQ_H_
#define LLARP_IBMQ_H_
#include <llarp/buffer.h>
#include <stdbool.h>
#include <llarp/msg_handler.h>

#ifdef __cplusplus
extern "C" {
#endif

  struct llarp_mq;
  
  struct llarp_mq * llarp_init_mq();
  void llarp_free_mq(struct llarp_mq ** queue);
  /** 
      offer a full message to the inbound message queue 
      return true if successfully added
      return false if the queue is full
   */
  bool llarp_mq_offer(struct llarp_mq * queue, llarp_buffer_t msg);
  size_t llarp_mq_peek(struct llarp_mq * queue);
  /** return true if we have more messages to process */
  bool llarp_mq_process(struct llarp_mq * queue, struct llarp_msg_muxer * muxer);

#ifdef __cplusplus
}
#endif

#endif
