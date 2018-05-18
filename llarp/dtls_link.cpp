#include <llarp/dtls.h>
#include <llarp/net.h>
#include <map>
#include "crypto.hpp"

struct dtls_session
{
};

struct dtls_link
{
  struct llarp_alloc * mem;
  struct llarp_logic * logic;
  struct llarp_ev_loop * netloop;
  struct llarp_msg_muxer * msghandler;
  struct llarp_udp_io udp;
  char keyfile[255];
  char certfile[255];
  uint32_t timeout_job_id;
  std::map<llarp::pubkey, llarp_link_session> sessions;
};

static struct dtls_link * dtls_link_alloc(struct llarp_alloc * mem, struct llarp_msg_muxer * muxer, const char * keyfile, const char * certfile)
{
  void * ptr = mem->alloc(mem, sizeof(struct dtls_link), 8);
  if(ptr)
  {
    struct dtls_link * link = new (ptr) dtls_link;
    link->mem = mem;
    link->msghandler = muxer;
    strncpy(link->keyfile, keyfile, sizeof(link->keyfile));
    strncpy(link->certfile, certfile, sizeof(link->certfile));
    return link;
  }
  return nullptr;
}


static const char * dtls_link_name()
{
  return "dtls";
}

// forward declare
static void dtls_link_cleanup_timer(void * l, uint64_t orig, uint64_t left);

static void dtls_link_issue_cleanup_timer(struct dtls_link * link, uint64_t timeout)
{
  struct llarp_timeout_job job = {
    .timeout = timeout,
    .user = link,
    .handler = &dtls_link_cleanup_timer
  };
  link->timeout_job_id = llarp_logic_call_later(link->logic, job);
}

static bool dtls_link_configure(struct llarp_link * l, struct llarp_ev_loop * netloop, const char * ifname, int af, uint16_t port)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  if(!llarp_getifaddr(ifname, af, &link->udp.addr))
    return false;
  link->netloop = netloop;
  return llarp_ev_add_udp(link->netloop, &link->udp) != -1;
}

static bool dtls_link_start(struct llarp_link * l, struct llarp_logic * logic)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  link->timeout_job_id = 0;
  link->logic = logic;
  // start cleanup timer
  dtls_link_issue_cleanup_timer(link, 1000); // every 1 second
  return true;
}

static void dtls_link_cleanup_dead_sessions(struct dtls_link * link)
{
  // TODO: implement
}

static void dtls_link_cleanup_timer(void * l, uint64_t orig, uint64_t left)
{
  dtls_link * link = static_cast<dtls_link*>(l);
  // clear out previous id of job
  link->timeout_job_id = 0;
  if(!left)
  {
    dtls_link_cleanup_dead_sessions(link);
    //TODO: exponential backoff for cleanup timer ?
    dtls_link_issue_cleanup_timer(link, orig);
  }
}


static bool dtls_link_stop(struct llarp_link *l)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  if(link->timeout_job_id)
  {
    llarp_logic_cancel_call(link->logic, link->timeout_job_id);
  }
  return true;
}


static void dtls_link_iter_sessions(struct llarp_link * l, struct llarp_link_session_iter * iter)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  iter->link = l;
  for (auto & item : link->sessions)
    if(!iter->visit(iter, &item.second)) return;
}


static void dtls_link_try_establish(struct llarp_link * link, struct llarp_link_establish_job job, struct llarp_link_session_listener l)
{
}

static void dtls_link_mark_session_active(struct llarp_link * link, struct llarp_link_session * s)
{
}

static struct llarp_link_session * dtls_link_session_for_addr(struct llarp_link * link, const struct sockaddr * saddr)
{
  return nullptr;
}

static void dtls_link_free(struct llarp_link *l)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  struct llarp_alloc * mem = link->mem;
  link->~dtls_link();
  mem->free(mem, link);
}


extern "C" {

void dtls_link_init(struct llarp_link * link, struct llarp_dtls_args args, struct llarp_msg_muxer * muxer)
{
  link->impl = dtls_link_alloc(args.mem, muxer, args.keyfile, args.certfile);
  link->name = dtls_link_name;
  /*
  link->register_listener = dtls_link_reg_listener;
  link->deregister_listener = dtls_link_dereg_listener;
  */
  link->configure = dtls_link_configure;
  link->start_link = dtls_link_start;
  link->stop_link = dtls_link_stop;
  link->iter_sessions = dtls_link_iter_sessions;
  link->try_establish = dtls_link_try_establish;
  link->acquire_session_for_addr = dtls_link_session_for_addr;
  link->mark_session_active = dtls_link_mark_session_active;
  link->free_impl = dtls_link_free;
}

}
