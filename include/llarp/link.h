#ifndef LLARP_LINK_H_
#define LLARP_LINK_H_
#include <llarp/crypto.h>
#include <llarp/mem.h>
#include <llarp/msg_handler.h>
#include <llarp/obmd.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_link;

struct llarp_link *llarp_link_alloc(struct llarp_msg_muxer *muxer);
void llarp_link_free(struct llarp_link **link);

bool llarp_link_configure_addr(struct llarp_link *link, const char *ifname,
                               int af, uint16_t port);

/** get link listener for events */
struct llarp_udp_listener *llarp_link_udp_listener(struct llarp_link *link);

void llarp_link_start(struct llarp_link *link);

struct llarp_link_queue *llarp_link_frame_queue(struct llarp_link *link);

void llarp_link_stop(struct llarp_link *link);

struct llarp_link_session;

struct llarp_link_session_listener {
  void *user;
  /** set by llarp_try_establish_session */
  struct llarp_link *link;
  /** set by llarp_try_establish_session */
  struct llarp_rc *rc;
  void (*result)(struct llarp_link_session_listener *,
                 struct llarp_link_session *);
};

/** information for establishing an outbound session */
struct llarp_link_establish_job {
  struct llarp_rc *rc;
  uint64_t timeout;
};

void llarp_link_try_establish_session(struct llarp_link *link,
                                      struct llarp_link_establish_job *job,
                                      struct llarp_link_session_listener *l);

struct llarp_link_session_iter {
  void *user;
  struct llarp_link *link;
  bool (*visit)(struct llarp_link_session_iter *, struct llarp_link_session *);
};

void llarp_link_iter_sessions(struct llarp_link *l,
                              struct llarp_link_session_iter *i);

struct llarp_rc *llarp_link_session_rc(struct llarp_link_session *s);

#ifdef __cplusplus
}
#endif

#endif
