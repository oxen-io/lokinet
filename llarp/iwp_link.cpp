#include <llarp/iwp.h>
#include <llarp/net.h>

#include <fstream>
#include <map>

#include "crypto.hpp"
#include "fs.hpp"
#include "net.hpp"

namespace iwp
{

struct session
{
};

struct server
{
  struct llarp_alloc * mem;
  struct llarp_logic * logic;
  struct llarp_crypto * crypto;
  struct llarp_ev_loop * netloop;
  struct llarp_msg_muxer * muxer;
  struct llarp_udp_io udp;
  char keyfile[255];
  uint32_t timeout_job_id;
  std::map<llarp::Addr, llarp_link_session> sessions;

  llarp_seckey_t seckey;
  
  void inbound_session(llarp::Addr & src)
  {
    
  }

  void cleanup_dead()
  {
    // todo: implement
  }

  bool ensure_privkey()
  {
    std::error_code ec;
    if(!fs::exists(keyfile, ec))
    {
      if(!keygen(keyfile))
        return false;
    }
    std::ifstream f(keyfile);
    if(f.is_open())
    {
      f.read((char*)seckey, sizeof(seckey));
      return true;
    }
    return false;
  }

  bool keygen(const char * fname)
  {
    crypto->keygen(&seckey);
    std::ofstream f(fname);
    if(f.is_open())
    {
      f.write((char*)seckey, sizeof(seckey));
      return true;
    }
    return false;
  }
  
  static void handle_cleanup_timer(void * l, uint64_t orig, uint64_t left)
  {
    server * link = static_cast<server *>(l);
    link->timeout_job_id = 0;
    if(!left)
    {
      link->cleanup_dead();
      //TODO: exponential backoff for cleanup timer ?
      link->issue_cleanup_timer(orig);
    }
  }

  static void handle_recvfrom(struct llarp_udp_io * udp, const struct sockaddr *saddr, void * buf, ssize_t sz)
  {
    server * link = static_cast<server *>(udp->user);
    llarp::Addr src = *saddr;
    auto itr = link->sessions.find(src);
    if (itr == link->sessions.end())
    {
      link->inbound_session(src);
    }
  }

  void cancel_timer()
  {
    if(timeout_job_id)
    {
      llarp_logic_cancel_call(logic, timeout_job_id);
    }
    timeout_job_id = 0;
  }
  
  void issue_cleanup_timer(uint64_t timeout)
  {
    llarp_timeout_job job = {
      .timeout = timeout,
      .user = this,
      .handler = &server::handle_cleanup_timer
    };
    timeout_job_id = llarp_logic_call_later(logic, job);
  }
  
};

  server * link_alloc(struct llarp_alloc * mem, struct llarp_msg_muxer * muxer, const char * keyfile, struct llarp_crypto * crypto)
{
  void * ptr = mem->alloc(mem, sizeof(struct server), 8);
  if(ptr)
  {
    server * link = new (ptr) server;
    link->mem = mem;
    link->crypto = crypto;
    link->muxer = muxer;
    strncpy(link->keyfile, keyfile, sizeof(link->keyfile));
    return link;
  }
  return nullptr;
}


const char * link_name()
{
  return "dtls";
}


bool link_configure(struct llarp_link * l, struct llarp_ev_loop * netloop, const char * ifname, int af, uint16_t port)
{
  server * link = static_cast<server*>(l->impl);

  if(!link->ensure_privkey())
  {
    printf("failed to ensure private key\n");
    return false;
  }
  
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
  link->udp.recvfrom = &server::handle_recvfrom;
  link->udp.user = link;
  return llarp_ev_add_udp(link->netloop, &link->udp) != -1;
}

bool link_start(struct llarp_link * l, struct llarp_logic * logic)
{
  server * link = static_cast<server*>(l->impl);
  link->timeout_job_id = 0;
  link->logic = logic;
  // start cleanup timer
  link->issue_cleanup_timer(1000);
  return true;
}


bool link_stop(struct llarp_link *l)
{
  server * link = static_cast<server*>(l->impl);
  link->cancel_timer();
  return true;
}


void link_iter_sessions(struct llarp_link * l, struct llarp_link_session_iter * iter)
{
  server * link = static_cast<server*>(l->impl);
  iter->link = l;
  for (auto & item : link->sessions)
    if(!iter->visit(iter, &item.second)) return;
}


void link_try_establish(struct llarp_link * link, struct llarp_link_establish_job job, struct llarp_link_session_listener l)
{
}

void link_mark_session_active(struct llarp_link * link, struct llarp_link_session * s)
{
}

struct llarp_link_session * link_session_for_addr(struct llarp_link * l, const struct sockaddr * saddr)
{
  if(saddr)
  {
    server * link = static_cast<server*>(l->impl);
    for(auto & session : link->sessions)
    {
      if(session.second.addr == *saddr) return &link->sessions[session.first];
    }
  }
  return nullptr;
}

void link_free(struct llarp_link *l)
{
  server * link = static_cast<server*>(l->impl);
  struct llarp_alloc * mem = link->mem;
  link->~server();
  mem->free(mem, link);
}
}

extern "C" {

void iwp_link_init(struct llarp_link * link, struct llarp_iwp_args args, struct llarp_msg_muxer * muxer)
{
  link->impl = iwp::link_alloc(args.mem, muxer, args.keyfile, args.crypto);
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
