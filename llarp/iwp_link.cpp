#include <llarp/iwp.h>
#include <llarp/net.h>
#include <llarp/crypto_async.h>

#include <cassert>
#include <fstream>
#include <map>
#include <vector>
#include <list>

#include "crypto.hpp"
#include "fs.hpp"
#include "net.hpp"

namespace iwp
{
  
struct session
{
  llarp_udp_io * udp;
  llarp_crypto * crypto;
  llarp_async_iwp * iwp;
  llarp_seckey_t eph_seckey;
  llarp_pubkey_t remote;
  llarp_sharedkey_t sessionkey;
  llarp_link_session_listener establish_listener = {nullptr, nullptr, nullptr, nullptr};
  llarp::Addr addr;
  iwp_async_gen_intro intro;
  iwp_async_gen_introack introack;

  uint8_t workbuf[1024];
  
  typedef std::vector<uint8_t> sendbuf_t;
  typedef std::list<sendbuf_t> sendqueue_t;
  
  sendqueue_t sendq;

  enum State
  {
    eInitial,
    eIntroSent,
    eIntroAckSent,
    eIntroAckRecv,
    eTokenOfferSent,
    eTokenOfferRecv,
    eTokenAckSent,
    eTokenAckRecv,
    eEstablished,
    eTimeout
  };

  State state;
  
  session(llarp_udp_io * u, llarp_async_iwp * i, llarp_crypto * c, const llarp::Addr & a) :
    udp(u),
    crypto(c),
    iwp(i),
    addr(a),
    state(eInitial)
  {
    c->keygen(eph_seckey);
  }

  static bool sendto(llarp_link_session * s, llarp_buffer_t msg)
  {
    session * self = static_cast<session *>(s->impl);
    self->sendq.emplace_back(msg.sz);
    memcpy(self->sendq.back().data(), msg.base, msg.sz);
    self->pump();
    return true;
  }

  // pump sending messages
  void pump()
  {
  }
  
  static void handle_recv(llarp_link_session * s, const void * buf, size_t sz)
  {
    session * self = static_cast<session *>(s->impl);
    switch (self->state)
    {
    case eIntroSent:
      // got intro ack
      self->on_intro_ack(buf, sz);
      return;
    default:
      // invalid state?
      return;
    }
  }

  static bool is_timedout(llarp_link_session * s)
  {
    return false;
  }

  static void close(llarp_link_session * s)
  {
    
  }

  void on_intro_ack(const void * buf, size_t sz)
  {
    printf("iwp intro ack\n");
  }

  static void handle_generated_intro(iwp_async_gen_intro * i)
  {
    session * link = static_cast<session *>(i->user);
    llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz);
    printf("sent introduce of size %ld\n", i->sz);
  }

  
  void introduce(llarp_pubkey_t pub)
  {
    memcpy(remote, pub, 32);
    intro.buf = workbuf;
    size_t w0sz = (rand() % 64);
    intro.sz = 128 + w0sz;
    // randomize w0
    if(w0sz)
      crypto->randbytes(intro.buf + 128, w0sz);
    
    intro.nonce = workbuf + 64;
    intro.secretkey = eph_seckey;
    intro.remote_pubkey = remote;
    // randomize nonce
    crypto->randbytes(intro.nonce, 32);
    // async generate intro packet
    intro.user = this;
    intro.hook = &handle_generated_intro;
    iwp_call_async_gen_intro(iwp, &intro);
  }
};

struct server
{
  struct llarp_alloc * mem;
  struct llarp_logic * logic;
  struct llarp_crypto * crypto;
  struct llarp_ev_loop * netloop;
  struct llarp_msg_muxer * muxer;
  struct llarp_async_iwp * iwp;
  struct llarp_udp_io udp;
  char keyfile[255];
  uint32_t timeout_job_id;
  std::map<llarp::Addr, llarp_link_session> sessions;

  llarp_seckey_t seckey;

  server(llarp_alloc * m, llarp_crypto * c, llarp_logic * l, llarp_threadpool * w)
  {
    mem = m;
    crypto = c;
    logic = l;
    iwp = llarp_async_iwp_new(mem, crypto, logic, w);
  }

  session * create_session(llarp::Addr & src)
  {
    session * impl = new session(&udp, iwp, crypto, src);
    llarp_link_session s;
    s.impl = impl;
    s.sendto = session::sendto;
    s.recv = session::handle_recv;
    s.timeout = session::is_timedout;
    s.close = session::close;
    sessions[src] = s;
    return impl;
  }
  
  void cleanup_dead()
  {
    // todo: implement
    printf("cleanup dead\n");
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
    crypto->keygen(seckey);
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
    else
    {
      printf("cleanup canceled\n");
    }
  }

  static void handle_recvfrom(struct llarp_udp_io * udp, const struct sockaddr *saddr, const void * buf, ssize_t sz)
  {
    server * link = static_cast<server *>(udp->user);
    llarp::Addr src = *saddr;
    auto itr = link->sessions.find(src);
    if (itr == link->sessions.end())
    {
      // new inbound session
      link->create_session(src);
    }
    auto & session = link->sessions[src];
    session.recv(&session, buf, sz);
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

server * link_alloc(struct llarp_alloc * mem, struct llarp_msg_muxer * muxer, const char * keyfile, struct llarp_crypto * crypto, struct llarp_logic * logic, struct llarp_threadpool * worker)
{
  void * ptr = mem->alloc(mem, sizeof(struct server), 8);
  if(ptr)
  {
    server * link = new (ptr) server(mem, crypto, logic, worker);
    link->muxer = muxer;
    strncpy(link->keyfile, keyfile, sizeof(link->keyfile));
    return link;
  }
  return nullptr;
}


const char * link_name()
{
  return "iwp";
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


void link_try_establish(struct llarp_link * l, struct llarp_link_establish_job job, struct llarp_link_session_listener listener)
{
  server * link = static_cast<server *>(l->impl);
  llarp::Addr dst(*job.ai);
  session * s = link->create_session(dst);
  s->establish_listener = listener;
  s->introduce(job.ai->enc_key);
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
  link->impl = iwp::link_alloc(args.mem, muxer, args.keyfile, args.crypto, args.logic, args.cryptoworker);
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
