#pragma once

#include "llarp/address_info.h"

struct llarp_link;
struct llarp_link_session;

struct llarp_link_establish_job
{
  void *user;
  void (*result)(struct llarp_link_establish_job *);
  struct llarp_ai ai;
  uint64_t timeout;
  uint16_t retries;

  byte_t pubkey[PUBKEYSIZE];
  /** set on success by try_establish */
  struct llarp_link *link;
  /** set on success by try_establish */
  struct llarp_link_session *session;
};