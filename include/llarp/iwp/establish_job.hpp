#pragma once

#include "llarp/address_info.hpp"

struct llarp_link;
struct llarp_link_session;

struct llarp_link_establish_job
{
  void *user;
  void (*result)(struct llarp_link_establish_job *);
  llarp::AddressInfo ai;
  uint64_t timeout;
  uint16_t retries;

  llarp::PubKey pubkey;
  /** set on success by try_establish */
  struct llarp_link *link;
  /** set on success by try_establish */
  struct llarp_link_session *session;
};
