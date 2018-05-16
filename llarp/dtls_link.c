#include <llarp/dtls.h>


struct dtls_link
{
  struct llarp_logic * logic;
  uint32_t timeout_job_id;
};

static struct dtls_link * dtls_link_alloc(struct llarp_msg_muxer * muxer, char * keyfile, char * certfile)
{
  struct dtls_link * link = llarp_g_mem.alloc(sizeof(struct dtls_link), 8);
  return link;
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

static bool dtls_link_start(struct llarp_link * l, struct llarp_logic * logic)
{
  struct dtls_link * link = l->impl;
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
  struct dtls_link * link = l;
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
  struct dtls_link * link = l->impl;
  if(link->timeout_job_id)
  {
    llarp_logic_cancel_call(link->logic, link->timeout_job_id);
  }
  return true;
}


static void dtls_link_iter_sessions(struct llarp_link * l, struct llarp_link_session_iter * iter)
{
  /*
  struct dtls_link * link = l->impl;
  struct llarp_link_session * session;
  iter->link = l;
  */
}


static void dtls_link_try_establish(struct llarp_link * link, struct llarp_link_establish_job job, struct llarp_link_session_listener l)
{
}

static void dtls_link_free(struct llarp_link *l)
{
  llarp_g_mem.free(l->impl);
}



void dtls_link_init(struct llarp_link * link, struct llarp_dtls_args args, struct llarp_msg_muxer * muxer)
{
  link->impl = dtls_link_alloc(muxer, args.key_file, args.cert_file);
  link->name = dtls_link_name;
  /*
  link->register_listener = dtls_link_reg_listener;
  link->deregister_listener = dtls_link_dereg_listener;
  */
  link->start_link = dtls_link_start;
  link->stop_link = dtls_link_stop;
  link->iter_sessions = dtls_link_iter_sessions;
  link->try_establish = dtls_link_try_establish;
  link->free_impl = dtls_link_free;
}
