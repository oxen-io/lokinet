#ifndef LLARP_LINK_H_
#define LLARP_LINK_H_
#include <llarp/crypto.h>
#include <llarp/mem.h>
#include <llarp/msg_handler.h>
#include <llarp/obmd.h>
#include <llarp/ev.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
  
/**
 * wire layer transport interface 
 */
struct llarp_link;

  
/**
 * wire layer transport session for point to point communication between us and another
 */
struct llarp_link_session;

struct llarp_link_session_listener {
  void *user;
  /** set by try_establish */
  struct llarp_link *link;
  /** set by try_establish */
  struct llarp_rc *rc;
  /** callback to handle result */
  void (*result)(struct llarp_link_session_listener *,
                 struct llarp_link_session *);
};

/** information for establishing an outbound session */
struct llarp_link_establish_job {
  struct llarp_rc *rc;
  uint64_t timeout;
};

struct llarp_link_session_iter {
  void *user;
  struct llarp_link *link;
  bool (*visit)(struct llarp_link_session_iter *, struct llarp_link_session *);
};

struct llarp_link
{
  void * impl;
  int (*configure_addr)(struct llarp_link *, const char *, int, uint16_t);
  int (*start_link)(struct llarp_link *, struct llarp_ev_loop *);
  int (*stop_link)(struct llarp_link *);
  struct llarp_rc * (*get_our_rc)(struct llarp_link *);
  void (*iter_sessions)(struct llarp_link *, struct llarp_link_session_iter);
  void (*try_establish)(struct llarp_link *,
                        struct llarp_link_establish_job,
                        struct llarp_link_session_listener);
  void (*free)(struct llarp_link *);
};

struct llarp_link_session
{
  void * impl;
  struct llarp_rc * (*get_remote_rc)(struct llarp_link_session *);
  /** send an entire message, splits up into smaller pieces and does encryption */
  ssize_t (*sendto)(struct llarp_link_session *, llarp_buffer_t);
};

  
#ifdef __cplusplus
}
#endif

#endif
