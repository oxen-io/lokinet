#ifndef LLARP_LINK_H_
#define LLARP_LINK_H_
#include <llarp/address_info.h>
#include <llarp/crypto.h>
#include <llarp/ev.h>
#include <llarp/logic.h>
#include <llarp/mem.h>
#include <llarp/msg_handler.h>
#include <llarp/obmd.h>

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
 * wire layer transport session for point to point communication between us and
 * another
 */
struct llarp_link_session;

/** outbound session establish job */
struct llarp_link_establish_job
{
  void *user;
  void (*result)(struct llarp_link_establish_job *);
  struct llarp_ai ai;
  uint64_t timeout;
  /** set on success by try_establish */
  struct llarp_link *link;
  /** set on success by try_establish */
  struct llarp_link_session *session;
};

struct llarp_link_session_iter
{
  void *user;
  struct llarp_link *link;
  bool (*visit)(struct llarp_link_session_iter *, struct llarp_link_session *);
};

struct llarp_link_ev_listener
{
  void *user;
  void (*established)(struct llarp_link_ev_listener *,
                      struct llarp_link_session *, bool);
  void (*timeout)(struct llarp_link_ev_listener *, struct llarp_link_session *,
                  bool);
  void (*tx)(struct llarp_link_ev_listener *, struct llarp_link_session *,
             size_t);
  void (*rx)(struct llarp_link_ev_listener *, struct llarp_link_session *,
             size_t);
  void (*error)(struct llarp_link_ev_listener *, struct llarp_link_session *,
                const char *);
};

struct llarp_link
{
  void *impl;
  const char *(*name)(void);
  void (*get_our_address)(struct llarp_link *, struct llarp_ai *);
  /*
  int (*register_listener)(struct llarp_link *, struct llarp_link_ev_listener);
  void (*deregister_listener)(struct llarp_link *, int);
  */
  bool (*configure)(struct llarp_link *, struct llarp_ev_loop *, const char *,
                    int, uint16_t);
  bool (*start_link)(struct llarp_link *, struct llarp_logic *);
  bool (*stop_link)(struct llarp_link *);
  void (*iter_sessions)(struct llarp_link *, struct llarp_link_session_iter);
  bool (*try_establish)(struct llarp_link *, struct llarp_link_establish_job *);

  /**
      struct llarp_link_session * (*acquire_session_for_addr)(struct llarp_link
     *, const struct sockaddr *);
  */
  void (*mark_session_active)(struct llarp_link *, struct llarp_link_session *);
  void (*free_impl)(struct llarp_link *);
};

/** checks if all members are initialized */
bool
llarp_link_initialized(struct llarp_link *link);

struct llarp_link_session
{
  void *impl;
  /** send an entire message, splits up into smaller pieces and does encryption
   */
  bool (*sendto)(struct llarp_link_session *, llarp_buffer_t);
  /** return true if this session is timed out */
  bool (*timeout)(struct llarp_link_session *);
  /** explicit close session */
  void (*close)(struct llarp_link_session *);
};

bool
llarp_link_session_initialized(struct llarp_link_session *s);

#ifdef __cplusplus
}
#endif
#endif
