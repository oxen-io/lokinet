#include <llarp/dtls.h>
#include <llarp/net.h>


#include <map>
#include "crypto.hpp"
#include "fs.hpp"
#include "net.hpp"

namespace iwp
{

struct dtls_session
{
};

struct dtls_link
{
  struct llarp_alloc * mem;
  struct llarp_logic * logic;
  struct llarp_ev_loop * netloop;
  struct llarp_msg_muxer * muxer;
  struct llarp_udp_io udp;
  char keyfile[255];
  char certfile[255];
  uint32_t timeout_job_id;
  std::map<llarp::Addr, llarp_link_session> sessions;

  dtls_link()
  {
    mbedtls_x509_crt_init( &servercert );
    mbedtls_pk_init( &privkey );
  }
  
  entropy_context entropy;
  ecdsa_context ecdsa;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_cookie_ctx cookie_ctx;
  mbedtls_x509_crt servercert;
  mbedtls_ssl_config conf;
  mbedtls_pk_context privkey;
  mbedtls_timing_delay_context timer;
  
  void inbound_session(llarp::Addr & src)
  {
    
  }
  
};

static struct dtls_link * dtls_link_alloc(struct llarp_alloc * mem, struct llarp_msg_muxer * muxer, const char * keyfile, const char * certfile)
{
  void * ptr = mem->alloc(mem, sizeof(struct dtls_link), 8);
  if(ptr)
  {
    struct dtls_link * link = new (ptr) dtls_link;
    link->mem = mem;
    link->muxer = muxer;
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

void dtls_recvfrom(struct llarp_udp_io * udp, const struct sockaddr *saddr, void * buf, ssize_t sz)
{
  dtls_link * link = static_cast<dtls_link *>(udp->user);
  llarp::Addr src = *saddr;
  auto itr = link->sessions.find(src);
  if (itr == link->sessions.end())
  {
    link->inbound_session(src);
  }
}


static bool dtls_link_configure(struct llarp_link * l, struct llarp_ev_loop * netloop, const char * ifname, int af, uint16_t port)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);

  if(!link->ensure_privkey())
    return false;

  if(!link->ensure_certfile())
    return false;

  // bind
  
  link->udp.addr.sa_family = af;
  if(!llarp_getifaddr(ifname, af, &link->udp.addr))
    return false;
  switch(af)
  {
  case AF_INET:
    ((sockaddr_in *)&link->udp.addr)->sin_port = htons(port);
    break;
  case AF_INET6:
    ((sockaddr_in6 *)(&link->udp.addr))->sin6_port = htons(port);
    break;
      // TODO: AF_PACKET
  default:
      return false;
  }
  link->netloop = netloop;
  link->udp.recvfrom = &dtls_recvfrom;
  link->udp.user = link;
  return llarp_ev_add_udp(link->netloop, &link->udp) != -1;
}

bool dtls_link_start(struct llarp_link * l, struct llarp_logic * logic)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  link->timeout_job_id = 0;
  link->logic = logic;
  // start cleanup timer
  dtls_link_issue_cleanup_timer(link, 1000); // every 1 second
  return true;
}

void dtls_link_cleanup_dead_sessions(struct dtls_link * link)
{
  // TODO: implement
}

void dtls_link_cleanup_timer(void * l, uint64_t orig, uint64_t left)
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


bool dtls_link_stop(struct llarp_link *l)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  if(link->timeout_job_id)
  {
    llarp_logic_cancel_call(link->logic, link->timeout_job_id);
  }
  return true;
}


void dtls_link_iter_sessions(struct llarp_link * l, struct llarp_link_session_iter * iter)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  iter->link = l;
  for (auto & item : link->sessions)
    if(!iter->visit(iter, &item.second)) return;
}


void dtls_link_try_establish(struct llarp_link * link, struct llarp_link_establish_job job, struct llarp_link_session_listener l)
{
}

void dtls_link_mark_session_active(struct llarp_link * link, struct llarp_link_session * s)
{
}

struct llarp_link_session * dtls_link_session_for_addr(struct llarp_link * l, const struct sockaddr * saddr)
{
  if(saddr)
  {
    dtls_link * link = static_cast<dtls_link*>(l->impl);
    for(auto & session : link->sessions)
    {
      if(session.second.addr == *saddr) return &link->sessions[session.first];
    }
  }
  return nullptr;
}

void dtls_link_free(struct llarp_link *l)
{
  dtls_link * link = static_cast<dtls_link*>(l->impl);
  struct llarp_alloc * mem = link->mem;
  link->~dtls_link();
  mem->free(mem, link);
}
}

extern "C" {

void iwp_link_init(struct llarp_link * link, struct llarp_iwp_args args, struct llarp_msg_muxer * muxer)
{
  link->impl = iwp::link_alloc(args.mem, muxer, args.keyfile, args.certfile);
  link->name = iwp::link_name;
  link->configure = iwp::link_configure;
  link->start_link = iwp::link_start;
  link->stop_link = iwp::link_stop;
  link->iter_sessions = iwp::link_iter_sessions;
  link->try_establish = iwp::link_try_establish;
  link->acquire_session_for_addr = iwp::link_session_for_addr;
  link->mark_session_active = iwp::link_mark_session_active;
  link->free_impl = iwp::link_free;
}

}
