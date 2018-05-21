#include <llarp/iwp.h>
#include <llarp/net.h>
#include <llarp/crypto_async.h>
#include <llarp/time.h>

#include <cassert>
#include <fstream>
#include <map>
#include <vector>
#include <bitset>
#include <mutex>
#include <list>

#include "crypto.hpp"
#include "fs.hpp"
#include "mem.hpp"
#include "net.hpp"

namespace iwp
{

  // session activity timeout is 10s
  constexpr llarp_time_t SESSION_TIMEOUT = 10000;
  
enum header_flag
{
  eSessionInvalidated = (1 << 0),
  eHighPacketDrop = (1 << 1),
  eHighMTUDetected = (1 << 2),
  eProtoUpgrade = (1 << 3)
};

enum msgtype
{
  eALIV = 0x00,
  eXMIT = 0x01,
  eACKS = 0x02,
  eFRAG = 0x03
};
  
/** plaintext frame header */
struct frame_header
{
  uint8_t * ptr;
  
  frame_header(uint8_t * buf) : ptr(buf)
  {
  }

  uint8_t * data()
  {
    return ptr + 4;
  }
  
  uint8_t & version()
  {
    return ptr[0];
  }

  uint8_t & msgtype()
  {
    return ptr[1];
  }

  // 12 bits
  uint16_t size() const
  {
    uint16_t sz = (ptr[3] | 0x00fc) << 8;
    sz |= ptr[2];
    return sz;
  }

  void setsize(uint16_t sz)
  {
    ptr[3] = (sz | 0xfc00) >> 8;
    ptr[2] = (sz | 0x00ff);
  }
  
  // 4 bits
  uint8_t flags() const
  {
    return ptr[3] & 0x07;
  }

  void setflag(header_flag f)
  {
    ptr[3] |= f;
  }
  
};
  
/** xmit header */
struct xmit
{
  uint32_t buffer[11];

  xmit() {}
  
  xmit(uint8_t * ptr)
  {
    memcpy(buffer, ptr, 44);
  }

  xmit(const xmit & other)
  {
    memcpy(buffer, other.buffer, 44);
  }
  
  uint64_t msgid() const
  {
    // big endian assumed
    // TODO: implement little endian
    const uint32_t * start = (buffer + 8);
    const uint64_t * msgid = (const uint64_t *) start;
    return *msgid;
  }

  // size of each full fragment
  uint16_t fragsize() const
  {
    // big endian assumed
    // TODO: implement little endian
    return ((buffer[10] & 0xfc000000) >> 20);
  }

  // number of full fragments
  uint8_t numfrags() const
  {
    return (buffer[10] & 0x07000000) >> 16;
  }

  // size of the entire message 
  size_t totalsize() const
  {
    return (fragsize() * numfrags()) + lastfrag();
  }

  // size of the last fragment
  uint8_t lastfrag() const
  {
    // big endian assumed
    // TODO: implement little endian
    return (buffer[10] & 0x0000ff00) >> 8;
  }

  uint8_t flags () const
  {
    // big endian assumed
    // TODO: implement little endian
    return (buffer[10] & 0x000000ff);
  }
  
};

typedef std::vector<uint8_t> fragment_t;

// forward declare
struct session;
  
struct transitframe
{
  session * parent = nullptr;
  xmit msginfo;
  std::bitset<16> status;

  std::map<uint16_t, fragment_t> frags;
  fragment_t lastfrag;
  
  transitframe() {}

  // inbound
  transitframe(const xmit & x) : msginfo(x)
  {
  }

  // outbound
  transitframe(const llarp_buffer_t & buf, session * s) :
    parent(s)
  {
  }

  void put_lastfrag(uint8_t * buf, size_t sz)
  {
    lastfrag.resize(sz);
    memcpy(lastfrag.data(), buf, sz);
  }
  
};
  
struct frame_state
{
  llarp_time_t lastEvent = 0;
  std::map<uint64_t, transitframe> rx;
  std::map<uint64_t, transitframe*> tx;

  bool got_xmit(frame_header & hdr, size_t sz)
  {
    if(hdr.size() > sz)
    {
      // overflow
      printf("invalid XMIT frame size\n");
      return false;
    }
    sz = hdr.size();
    // mark we are alive
    alive();

    // extract xmit data
    xmit x(hdr.data());
    
    if(sz - 44 != x.lastfrag())
    {
      // bad size of last fragment
      printf("XMIT frag size missmatch, %ld != %d\n", sz - 44, x.lastfrag());
      return false;
    }
    
    if(x.flags() & 0x80)
    {
      auto itr = rx.try_emplace(x.msgid(), x);
      if(itr.second)
      {
        // inserted, put last fragment
        itr.first->second.put_lastfrag(hdr.data() + 44, x.lastfrag());
        return true;
      }
      else
        printf("duplicate XMIT msgid=%ld\n", x.msgid());
    }
    else
      printf("XMIT flags MSB not set\n");
    return false;
  }

  void alive()
  {
    lastEvent = llarp_time_now_ms();
  }

  bool got_frag(frame_header & hdr, size_t sz)
  {
    return false;
  }
  
  bool got_acks(frame_header & hdr, size_t sz)
  {
    return false;
  }

  // queue new outbound message
  void queue_tx(transitframe * frame)
  {
    
  }

  // get next frame to encrypt and transmit
  bool next_frame(llarp_buffer_t & buf)
  {
    return false;
  }
  
  bool process(uint8_t * buf, size_t sz)
  {
    frame_header hdr(buf);
    switch(hdr.msgtype())
    {
    case eALIV:
      alive();
      return true;
    case eXMIT:
      return got_xmit(hdr, sz - 4);
    case eACKS:
      return got_acks(hdr, sz - 4);
    case eFRAG:
      return got_frag(hdr, sz - 4);
    default:
      return false;
    }
  }
};

struct session
{
  llarp_alloc * mem;
  llarp_msg_muxer * muxer;
  llarp_udp_io * udp;
  llarp_crypto * crypto;
  llarp_async_iwp * iwp;
  llarp_logic * logic;
  llarp_seckey_t eph_seckey;
  llarp_pubkey_t remote;
  llarp_sharedkey_t sessionkey;
  llarp_link_session_listener establish_listener = {nullptr, nullptr, nullptr, nullptr};
  llarp::Addr addr;
  iwp_async_intro intro;
  iwp_async_introack introack;
  iwp_async_session_start start;
  frame_state frame;

  uint8_t token[32];
  uint8_t workbuf[256];

  enum State
  {
    eInitial,
    eIntroSent,
    eIntroAckSent,
    eIntroAckRecv,
    eEstablished,
    eTimeout
  };

  State state;
  
  session(llarp_alloc * m, llarp_msg_muxer * mux, llarp_udp_io * u, llarp_async_iwp * i, llarp_crypto * c, llarp_logic * l, const llarp::Addr & a) :
    mem(m),
    muxer(mux),
    udp(u),
    crypto(c),
    iwp(i),
    logic(l),
    addr(a),
    state(eInitial)
  {
    c->keygen(eph_seckey);
  }

  static void handle_sendto(void * user)
  {
    transitframe * frame = static_cast<transitframe*>(user);
    frame->parent->frame.queue_tx(frame);
  }

      
  static bool sendto(llarp_link_session * s, llarp_buffer_t msg)
  {
    session * self = static_cast<session *>(s->impl);
    void * ptr = self->mem->alloc(self->mem, sizeof(transitframe), 64);
    transitframe * frame = new (ptr) transitframe(msg, self);
    llarp_thread_job job = {
      .user = frame,
      .work = &handle_sendto
    };
    llarp_logic_queue_job(self->logic, job);
    return true;
  }

  void pump()
  {
    llarp_buffer_t buf;
    while(frame.next_frame(buf))
    {
      encrypt_frame_async_send(buf.base, buf.sz);
    }
  }

  // this is called from net thread
  void recv(const void * buf, size_t sz)
  {
    switch (state)
    {
    case eIntroSent:
      // got intro ack
      on_intro_ack(buf, sz);
      return;
    case eEstablished:
      // session is started
      decrypt_frame(buf, sz);
    default:
      // invalid state?
      return;
    }
  }


  bool timedout(llarp_time_t now, llarp_time_t timeout=SESSION_TIMEOUT)
  {
    return now - frame.lastEvent >= timeout;
  }
  
  static bool is_timedout(llarp_link_session * s)
  {
    auto now = llarp_time_now_ms();
    return static_cast<session*>(s->impl)->timedout(now);
  }

  static void close(llarp_link_session * s)
  {
    // TODO: implement
  }

  static void handle_verify_introack(iwp_async_introack * introack)
  {
    session * link = static_cast<session *>(introack->user);
    if(introack->buf == nullptr)
    {
      // invalid signature
      printf("introack validation failed\n");
      return;
    }
    printf("introack validated\n");
    link->state = eIntroAckRecv;
    // copy decrypted token
    memcpy(link->token, introack->token, 32);
    link->session_start();
  }

  static void handle_generated_session_start(iwp_async_session_start * start)
  {
    session * link = static_cast<session*>(start->user);
    llarp_ev_udp_sendto(link->udp, link->addr, start->buf, start->sz);
    link->state = eEstablished;
    printf("session start sent\n");
  }

  void session_start()
  {
    size_t w2sz = rand() % 32;
    start.buf = workbuf;
    start.sz = w2sz + (32 * 3);
    start.nonce = workbuf + 32;
    crypto->randbytes(start.nonce, 32);
    start.token = token;
    memcpy(start.buf + 64, token, 32);
    if(w2sz)
      crypto->randbytes(start.buf + (32 * 3), w2sz);
    start.sessionkey = sessionkey;
    start.user = this;
    start.hook = &handle_generated_session_start; 
    iwp_call_async_gen_session_start(iwp, &start);
  }

  static void handle_frame_decrypt(iwp_async_frame * frame)
  {
    session * self = static_cast<session *>(frame->user);
    if(frame->success)
    {
      self->frame.process(frame->buf + 64, frame->sz - 64);
    }
    else
      printf("decrypt frame fail\n");

    self->mem->free(self->mem, frame);
  }
  
  void decrypt_frame(const void * buf, size_t sz)
  {
    if(sz > 64)
    {
      printf("decrypt frame of size %ld\n", sz);
      auto frame = alloc_frame(buf, sz);
      frame->hook = &handle_frame_decrypt;
      iwp_call_async_frame_decrypt(iwp, frame);
    }
    else
      printf("short packet of size %ld\n", sz);
  }

  static void handle_frame_encrypt(iwp_async_frame * frame)
  {
    session * self = static_cast<session *>(frame->user);
    printf("sendto %ld\n", frame->sz);
    llarp_ev_udp_sendto(self->udp, self->addr, frame->buf, frame->sz);
    self->mem->free(self->mem, frame);
  }

  iwp_async_frame * alloc_frame(const void * buf, size_t sz)
  {
    iwp_async_frame * frame = (iwp_async_frame*) mem->alloc(mem, sizeof(iwp_async_frame), 2048);
    memcpy(frame->buf, buf, sz);
    frame->sz = sz;
    frame->user = this;
    frame->sessionkey = sessionkey;
    return frame;
  }
  
  void encrypt_frame_async_send(const void * buf, size_t sz)
  {
    printf("encrypt frame of size %ld\n", sz);
    auto frame = alloc_frame(buf, sz);
    frame->hook = &handle_frame_encrypt;
    iwp_call_async_frame_encrypt(iwp, frame);
  }
  
  void on_intro_ack(const void * buf, size_t sz)
  {
    printf("iwp intro ack\n");
    if(sz >= sizeof(workbuf))
    {
      // too big?
      printf("intro ack too big\n");
      // TOOD: session destroy ?
      return;
    }
    // copy buffer so we own it
    memcpy(workbuf, buf, sz);
    // set intro ack parameters
    introack.buf = workbuf;
    introack.sz = sz;
    introack.nonce = workbuf + 64;
    introack.remote_pubkey = remote;
    introack.secretkey = eph_seckey;
    introack.user = this;
    introack.hook = &handle_verify_introack;
    // async verify
    iwp_call_async_verify_introack(iwp, &introack);
  }

  static void handle_generated_intro(iwp_async_intro * i)
  {
    session * link = static_cast<session *>(i->user);
    llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz);
    printf("sent introduce of size %ld\n", i->sz);
    link->state = eIntroSent;
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

  typedef std::mutex mtx_t;
  typedef std::lock_guard<mtx_t> lock_t;
  
  llarp_alloc * mem;
  llarp_logic * logic;
  llarp_crypto * crypto;
  llarp_ev_loop * netloop;
  llarp_msg_muxer * muxer;
  llarp_async_iwp * iwp;
  llarp_udp_io udp;
  char keyfile[255];
  uint32_t timeout_job_id;

  typedef std::map<llarp::Addr, llarp_link_session> LinkMap_t;
  
  LinkMap_t m_sessions;
  mtx_t m_sessions_Mutex;

  llarp_seckey_t seckey;

  server(llarp_alloc * m, llarp_crypto * c, llarp_logic * l, llarp_threadpool * w)
  {
    mem = m;
    crypto = c;
    logic = l;
    iwp = llarp_async_iwp_new(mem, crypto, logic, w);
  }

  session * create_session(const llarp::Addr & src)
  {
    return new session(mem, muxer, &udp, iwp, crypto, logic, src);
  }

  void put_session(const llarp::Addr & src, session * impl)
  {
    llarp_link_session s;
    llarp::Zero(&s, sizeof(s));
    src.CopyInto(s.addr);
    s.impl = impl;
    s.sendto = &session::sendto;
    s.timeout = &session::is_timedout;
    s.close = &session::close;
    {
      lock_t lock(m_sessions_Mutex);
      m_sessions[src] = s;
    }
  }
  
  session * ensure_session(const llarp::Addr & src)
  {
    session * s = nullptr;
    bool put = false;
    // TODO: will this be a bottleneck since it's called in a hot path?
    {
      lock_t lock(m_sessions_Mutex);
      auto itr = m_sessions.find(src);
      if (itr == m_sessions.end())
      {
        // new inbound session
        s = create_session(src);
        put = true;
      }
      else
        s = static_cast<session*>(itr->second.impl);
    }
    if(put)
      put_session(src, s);
    return s;
  }

  
  void cleanup_dead()
  {
    auto now = llarp_time_now_ms();
    std::vector<llarp::Addr> remove;
    printf("cleanup dead at %ld\n", now);
    {
      lock_t lock(m_sessions_Mutex);
      for (auto & itr : m_sessions)
      {
        session * s = static_cast<session *>(itr.second.impl);
        if(s->timedout(now))
          remove.push_back(itr.first);
      }
      
      for (const auto & addr : remove)
      {
        auto itr = m_sessions.find(addr);
        if(itr != m_sessions.end())
        {
          session * s = static_cast<session *>(itr->second.impl);
          m_sessions.erase(addr);
          delete s;
        }
      }
    }
  }

  uint8_t * pubkey()
  {
    return llarp_seckey_topublic(seckey);
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
  }

  // this is called in net threadpool
  static void handle_recvfrom(struct llarp_udp_io * udp, const struct sockaddr *saddr, const void * buf, ssize_t sz)
  {
    server * link = static_cast<server *>(udp->user);
    llarp::Addr src = *saddr;
    session * s = link->ensure_session(src);
    s->recv(buf, sz);
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
  return "IWP";
}


void link_get_addr(struct llarp_link * l, struct llarp_ai * addr)
{
  server * link = static_cast<server *>(l->impl);
  llarp::Addr linkaddr(link->udp.addr);
  addr->rank = 1;
  strncpy(addr->dialect, link_name(), sizeof(addr->dialect));
  memcpy(addr->enc_key, link->pubkey(), 32);
  memcpy(addr->ip.s6_addr, linkaddr.addr.s6_addr, 16);
  addr->port = linkaddr.port;
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
  // TODO: race condition with cleanup timer
  server::LinkMap_t copy = link->m_sessions;
  for (auto & item : copy)
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
  // TODO: implement
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
  link->get_our_address = iwp::link_get_addr;
  link->configure = iwp::link_configure;
  link->start_link = iwp::link_start;
  link->stop_link = iwp::link_stop;
  link->iter_sessions = iwp::link_iter_sessions;
  link->try_establish = iwp::link_try_establish;
  link->mark_session_active = iwp::link_mark_session_active;
  link->free_impl = iwp::link_free;
}

}
