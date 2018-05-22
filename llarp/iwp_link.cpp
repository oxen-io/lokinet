#include <llarp/crypto_async.h>
#include <llarp/iwp.h>
#include <llarp/net.h>
#include <llarp/time.h>

#include <bitset>
#include <cassert>
#include <fstream>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <vector>

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
    eHighPacketDrop     = (1 << 1),
    eHighMTUDetected    = (1 << 2),
    eProtoUpgrade       = (1 << 3)
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
    uint8_t *ptr;

    frame_header(uint8_t *buf) : ptr(buf)
    {
    }

    uint8_t *
    data()
    {
      return ptr + 4;
    }

    uint8_t &
    version()
    {
      return ptr[0];
    }

    uint8_t &
    msgtype()
    {
      return ptr[1];
    }

    // 12 bits
    uint16_t
    size() const
    {
      uint16_t sz = (ptr[3] | 0x00fc) << 8;
      sz |= ptr[2];
      return sz;
    }

    void
    setsize(uint16_t sz)
    {
      ptr[3] = (sz | 0xfc00) >> 8;
      ptr[2] = (sz | 0x00ff);
    }

    // 4 bits
    uint8_t
    flags() const
    {
      return ptr[3] & 0x07;
    }

    void
    setflag(header_flag f)
    {
      ptr[3] |= f;
    }
  };

  /** xmit header */
  struct xmit
  {
    uint32_t buffer[11];

    xmit()
    {
    }

    xmit(uint8_t *ptr)
    {
      memcpy(buffer, ptr, 44);
    }

    xmit(const xmit &other)
    {
      memcpy(buffer, other.buffer, 44);
    }

    uint64_t
    msgid() const
    {
      // big endian assumed
      // TODO: implement little endian
      const uint32_t *start = (buffer + 8);
      const uint64_t *msgid = (const uint64_t *)start;
      return *msgid;
    }

    // size of each full fragment
    uint16_t
    fragsize() const
    {
      // big endian assumed
      // TODO: implement little endian
      return ((buffer[10] & 0xfc000000) >> 20);
    }

    // number of full fragments
    uint8_t
    numfrags() const
    {
      return (buffer[10] & 0x07000000) >> 16;
    }

    // size of the entire message
    size_t
    totalsize() const
    {
      return (fragsize() * numfrags()) + lastfrag();
    }

    // size of the last fragment
    uint8_t
    lastfrag() const
    {
      // big endian assumed
      // TODO: implement little endian
      return (buffer[10] & 0x0000ff00) >> 8;
    }

    uint8_t
    flags() const
    {
      // big endian assumed
      // TODO: implement little endian
      return (buffer[10] & 0x000000ff);
    }
  };

  typedef std::vector< uint8_t > fragment_t;

  // forward declare
  struct session;

  struct transitframe
  {
    session *parent = nullptr;
    xmit msginfo;
    std::bitset< 16 > status;

    std::map< uint16_t, fragment_t > frags;
    fragment_t lastfrag;

    transitframe()
    {
    }

    // inbound
    transitframe(const xmit &x) : msginfo(x)
    {
    }

    // outbound
    transitframe(const llarp_buffer_t &buf, session *s) : parent(s)
    {
    }

    void
    put_lastfrag(uint8_t *buf, size_t sz)
    {
      lastfrag.resize(sz);
      memcpy(lastfrag.data(), buf, sz);
    }
  };

  struct frame_state
  {
    uint64_t ids           = 0;
    llarp_time_t lastEvent = 0;
    std::map< uint64_t, transitframe > rx;
    std::map< uint64_t, transitframe * > tx;

    typedef std::vector< uint8_t > sendbuf_t;
    std::queue< sendbuf_t > sendqueue;

    void
    init_sendbuf(sendbuf_t &buf, msgtype t, uint16_t sz, uint8_t flags)
    {
      buf.resize(4 + sz);
      buf[0] = 0;
      buf[1] = t;
      buf[2] = (sz & 0x00ff);
      buf[3] = flags;
    }

    void
    push_ackfor(uint64_t id, uint16_t bitmask)
    {
      sendbuf_t buf;
      // TODO: set flags to nonzero as needed
      init_sendbuf(buf, eACKS, 10, 0);
      // TODO: this assumes big endian
      memcpy(buf.data() + 4, &id, 8);
      memcpy(buf.data() + 12, &bitmask, 2);
      sendqueue.push(buf);
    }

    bool
    got_xmit(frame_header &hdr, size_t sz)
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

      // check MSB set on flags
      if(x.flags() & 0x80)
      {
        if(x.numfrags() > 0)
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
        {
          // short XMIT , no fragments so just ack
          push_ackfor(x.msgid(), 0);
        }
      }
      else
        printf("XMIT flags MSB not set\n");
      return false;
    }

    void
    alive()
    {
      lastEvent = llarp_time_now_ms();
    }

    bool
    got_frag(frame_header &hdr, size_t sz)
    {
      return false;
    }

    bool
    got_acks(frame_header &hdr, size_t sz)
    {
      return false;
    }

    // queue new outbound message
    void
    queue_tx(transitframe *frame)
    {
      ids++;
      tx.try_emplace(ids, frame);
    }

    // get next frame to encrypt and transmit
    bool
    next_frame(llarp_buffer_t &buf)
    {
      if(sendqueue.size())
      {
        auto &send = sendqueue.front();
        buf.base   = send.data();
        buf.cur    = send.data();
        buf.sz     = send.size();
        return true;
      }
      return false;
    }

    void
    pop_next_frame()
    {
      sendqueue.pop();
    }

    bool
    process(uint8_t *buf, size_t sz)
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
    llarp_alloc *mem;
    llarp_msg_muxer *muxer;
    llarp_udp_io *udp;
    llarp_crypto *crypto;
    llarp_async_iwp *iwp;
    llarp_logic *logic;
    llarp_seckey_t eph_seckey;
    llarp_pubkey_t remote;
    llarp_sharedkey_t sessionkey;

    llarp_link_establish_job *establish_job = nullptr;

    llarp::Addr addr;
    iwp_async_intro intro;
    iwp_async_introack introack;
    iwp_async_session_start start;
    frame_state frame;

    byte_t token[32];
    byte_t workbuf[2048];

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

    session(llarp_alloc *m, llarp_msg_muxer *mux, llarp_udp_io *u,
            llarp_async_iwp *i, llarp_crypto *c, llarp_logic *l,
            const llarp::Addr &a)
        : mem(m)
        , muxer(mux)
        , udp(u)
        , crypto(c)
        , iwp(i)
        , logic(l)
        , addr(a)
        , state(eInitial)
    {
      c->keygen(eph_seckey);
    }

    ~session()
    {
      printf("~session()\n");
    }

    static void
    handle_sendto(void *user)
    {
      transitframe *frame = static_cast< transitframe * >(user);
      frame->parent->frame.queue_tx(frame);
    }

    static bool
    sendto(llarp_link_session *s, llarp_buffer_t msg)
    {
      session *self = static_cast< session * >(s->impl);
      void *ptr     = self->mem->alloc(self->mem, sizeof(transitframe), 64);
      transitframe *frame  = new(ptr) transitframe(msg, self);
      llarp_thread_job job = {.user = frame, .work = &handle_sendto};
      llarp_logic_queue_job(self->logic, job);
      return true;
    }

    void
    pump()
    {
      llarp_buffer_t buf;
      while(frame.next_frame(buf))
      {
        encrypt_frame_async_send(buf.base, buf.sz);
      }
    }

    // this is called from net thread
    void
    recv(const void *buf, size_t sz)
    {
      switch(state)
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

    bool
    timedout(llarp_time_t now, llarp_time_t timeout = SESSION_TIMEOUT)
    {
      return now - frame.lastEvent >= timeout;
    }

    static bool
    is_timedout(llarp_link_session *s)
    {
      auto now = llarp_time_now_ms();
      return static_cast< session * >(s->impl)->timedout(now);
    }

    static void
    close(llarp_link_session *s)
    {
      // TODO: implement
    }

    static void
    handle_verify_introack(iwp_async_introack *introack)
    {
      session *link = static_cast< session * >(introack->user);
      if(introack->buf == nullptr)
      {
        // invalid signature
        printf("introack validation failed\n");
        return;
      }
      printf("introack validated\n");
      link->EnterState(eIntroAckRecv);
      // copy decrypted token
      memcpy(link->token, introack->token, 32);
      link->session_start();
    }

    static void
    handle_generated_session_start(iwp_async_session_start *start)
    {
      session *link = static_cast< session * >(start->user);
      llarp_ev_udp_sendto(link->udp, link->addr, start->buf, start->sz);
      link->EnterState(eEstablished);
      printf("session start sent\n");
    }

    void
    session_start()
    {
      size_t w2sz = rand() % 32;
      start.buf   = workbuf;
      start.sz    = w2sz + (32 * 3);
      start.nonce = workbuf + 32;
      crypto->randbytes(start.nonce, 32);
      start.token = token;
      memcpy(start.buf + 64, token, 32);
      if(w2sz)
        crypto->randbytes(start.buf + (32 * 3), w2sz);
      start.sessionkey = sessionkey;
      start.user       = this;
      start.hook       = &handle_generated_session_start;
      iwp_call_async_gen_session_start(iwp, &start);
    }

    static void
    handle_frame_decrypt(iwp_async_frame *frame)
    {
      session *self = static_cast< session * >(frame->user);
      if(frame->success)
      {
        self->frame.process(frame->buf + 64, frame->sz - 64);
      }
      else
        printf("decrypt frame fail\n");

      self->mem->free(self->mem, frame);
    }

    void
    decrypt_frame(const void *buf, size_t sz)
    {
      if(sz > 64)
      {
        printf("decrypt frame of size %ld\n", sz);
        auto frame  = alloc_frame(buf, sz);
        frame->hook = &handle_frame_decrypt;
        iwp_call_async_frame_decrypt(iwp, frame);
      }
      else
        printf("short packet of size %ld\n", sz);
    }

    static void
    handle_frame_encrypt(iwp_async_frame *frame)
    {
      session *self = static_cast< session * >(frame->user);
      printf("sendto %ld\n", frame->sz);
      llarp_ev_udp_sendto(self->udp, self->addr, frame->buf, frame->sz);
      self->mem->free(self->mem, frame);
    }

    iwp_async_frame *
    alloc_frame(const void *buf, size_t sz)
    {
      iwp_async_frame *frame =
          (iwp_async_frame *)mem->alloc(mem, sizeof(iwp_async_frame), 2048);
      memcpy(frame->buf, buf, sz);
      frame->sz         = sz;
      frame->user       = this;
      frame->sessionkey = sessionkey;
      return frame;
    }

    void
    encrypt_frame_async_send(const void *buf, size_t sz)
    {
      printf("encrypt frame of size %ld\n", sz);
      auto frame  = alloc_frame(buf, sz);
      frame->hook = &handle_frame_encrypt;
      iwp_call_async_frame_encrypt(iwp, frame);
    }

    void
    on_intro_ack(const void *buf, size_t sz)
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
      introack.buf           = workbuf;
      introack.sz            = sz;
      introack.nonce         = workbuf + 32;
      introack.remote_pubkey = remote;
      introack.secretkey     = eph_seckey;
      introack.user          = this;
      introack.hook          = &handle_verify_introack;
      // async verify
      iwp_call_async_verify_introack(iwp, &introack);
    }

    static void
    handle_generated_intro(iwp_async_intro *i)
    {
      session *link = static_cast< session * >(i->user);
      if(i->buf)
      {
        printf("sending...\n");
        llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz);
        printf("sent introduce of size %ld\n", i->sz);
        link->EnterState(eIntroSent);
      }
    }

    void
    introduce(uint8_t *pub)
    {
      memcpy(remote, pub, 32);
      intro.buf   = workbuf;
      size_t w0sz = (rand() % 32);
      intro.sz    = (32 * 3) + w0sz;
      // randomize w0
      if(w0sz)
      {
        printf("random padding %ld bytes\n", w0sz);
        crypto->randbytes(intro.buf + (32 * 3), w0sz);
      }

      intro.nonce         = workbuf + 32;
      intro.secretkey     = eph_seckey;
      intro.remote_pubkey = remote;
      // randomize nonce
      crypto->randbytes(intro.nonce, 32);
      // async generate intro packet
      intro.user = this;
      intro.hook = &handle_generated_intro;
      iwp_call_async_gen_intro(iwp, &intro);
    }

    void
    EnterState(State st)
    {
      if(state == eInitial)
        frame.alive();
      state = st;
    }
  };

  struct server
  {
    typedef std::mutex mtx_t;
    typedef std::lock_guard< mtx_t > lock_t;

    llarp_alloc *mem;
    llarp_logic *logic;
    llarp_crypto *crypto;
    llarp_ev_loop *netloop;
    llarp_msg_muxer *muxer;
    llarp_async_iwp *iwp;
    llarp_udp_io udp;
    char keyfile[255];
    uint32_t timeout_job_id;

    typedef std::map< llarp::Addr, llarp_link_session > LinkMap_t;

    LinkMap_t m_sessions;
    mtx_t m_sessions_Mutex;

    llarp_seckey_t seckey;

    server(llarp_alloc *m, llarp_crypto *c, llarp_logic *l, llarp_threadpool *w)
    {
      mem    = m;
      crypto = c;
      logic  = l;
      iwp    = llarp_async_iwp_new(mem, crypto, logic, w);
    }

    session *
    create_session(const llarp::Addr &src)
    {
      return new session(mem, muxer, &udp, iwp, crypto, logic, src);
    }

    bool
    has_session_to(const llarp::Addr &dst)
    {
      lock_t lock(m_sessions_Mutex);
      return m_sessions.find(dst) != m_sessions.end();
    }

    void
    put_session(const llarp::Addr &src, session *impl)
    {
      llarp_link_session s = {};
      src.CopyInto(s.addr);
      s.impl    = impl;
      s.sendto  = &session::sendto;
      s.timeout = &session::is_timedout;
      s.close   = &session::close;
      {
        lock_t lock(m_sessions_Mutex);
        m_sessions[src] = s;
      }
    }

    session *
    ensure_session(const llarp::Addr &src)
    {
      session *s = nullptr;
      bool put   = false;
      // TODO: will this be a bottleneck since it's called in a hot path?
      {
        lock_t lock(m_sessions_Mutex);
        auto itr = m_sessions.find(src);
        if(itr == m_sessions.end())
        {
          // new inbound session
          s   = create_session(src);
          put = true;
        }
        else
          s = static_cast< session * >(itr->second.impl);
      }
      if(put)
        put_session(src, s);
      return s;
    }

    void
    cleanup_dead()
    {
      auto now = llarp_time_now_ms();
      std::set< llarp::Addr > remove;
      printf("cleanup dead at %ld\n", now);
      {
        lock_t lock(m_sessions_Mutex);
        for(auto &itr : m_sessions)
        {
          session *s = static_cast< session * >(itr.second.impl);
          if(s->timedout(now))
            remove.insert(itr.first);
        }

        for(const auto &addr : remove)
        {
          auto itr = m_sessions.find(addr);
          if(itr != m_sessions.end())
          {
            session *s = static_cast< session * >(itr->second.impl);
            m_sessions.erase(addr);
            delete s;
          }
        }
      }
    }

    uint8_t *
    pubkey()
    {
      return llarp_seckey_topublic(seckey);
    }

    bool
    ensure_privkey()
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
        f.read((char *)seckey, sizeof(seckey));
        return true;
      }
      return false;
    }

    bool
    keygen(const char *fname)
    {
      crypto->keygen(seckey);
      std::ofstream f(fname);
      if(f.is_open())
      {
        f.write((char *)seckey, sizeof(seckey));
        return true;
      }
      return false;
    }

    static void
    handle_cleanup_timer(void *l, uint64_t orig, uint64_t left)
    {
      server *link         = static_cast< server * >(l);
      link->timeout_job_id = 0;
      if(!left)
      {
        link->cleanup_dead();
        // TODO: exponential backoff for cleanup timer ?
        link->issue_cleanup_timer(orig);
      }
    }

    // this is called in net threadpool
    static void
    handle_recvfrom(struct llarp_udp_io *udp, const struct sockaddr *saddr,
                    const void *buf, ssize_t sz)
    {
      server *link     = static_cast< server * >(udp->user);
      llarp::Addr addr = *saddr;
      session *s       = link->ensure_session(addr);
      s->recv(buf, sz);
    }

    void
    cancel_timer()
    {
      if(timeout_job_id)
      {
        llarp_logic_cancel_call(logic, timeout_job_id);
      }
      timeout_job_id = 0;
    }

    void
    issue_cleanup_timer(uint64_t timeout)
    {
      timeout_job_id = llarp_logic_call_later(
          logic, {timeout, this, &server::handle_cleanup_timer});
    }
  };

  server *
  link_alloc(struct llarp_alloc *mem, struct llarp_msg_muxer *muxer,
             const char *keyfile, struct llarp_crypto *crypto,
             struct llarp_logic *logic, struct llarp_threadpool *worker)
  {
    server *link = new server(mem, crypto, logic, worker);
    link->muxer  = muxer;
    strncpy(link->keyfile, keyfile, sizeof(link->keyfile));
    return link;
  }

  const char *
  link_name()
  {
    return "IWP";
  }

  void
  link_get_addr(struct llarp_link *l, struct llarp_ai *addr)
  {
    server *link = static_cast< server * >(l->impl);
    llarp::Addr linkaddr(link->udp.addr);
    addr->rank = 1;
    strncpy(addr->dialect, link_name(), sizeof(addr->dialect));
    memcpy(addr->enc_key, link->pubkey(), 32);
    memcpy(addr->ip.s6_addr, linkaddr.addr.s6_addr, 16);
    addr->port = linkaddr.port;
  }

  bool
  link_configure(struct llarp_link *l, struct llarp_ev_loop *netloop,
                 const char *ifname, int af, uint16_t port)
  {
    server *link = static_cast< server * >(l->impl);

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
    link->netloop      = netloop;
    link->udp.recvfrom = &server::handle_recvfrom;
    link->udp.user     = link;
    return llarp_ev_add_udp(link->netloop, &link->udp) != -1;
  }

  bool
  link_start(struct llarp_link *l, struct llarp_logic *logic)
  {
    server *link         = static_cast< server * >(l->impl);
    link->timeout_job_id = 0;
    link->logic          = logic;
    // start cleanup timer
    link->issue_cleanup_timer(1000);
    return true;
  }

  bool
  link_stop(struct llarp_link *l)
  {
    server *link = static_cast< server * >(l->impl);
    link->cancel_timer();
    return true;
  }

  void
  link_iter_sessions(struct llarp_link *l, struct llarp_link_session_iter iter)
  {
    server *link = static_cast< server * >(l->impl);
    iter.link    = l;
    // TODO: race condition with cleanup timer
    for(auto &item : link->m_sessions)
      if(!iter.visit(&iter, &item.second))
        return;
  }

  bool
  link_try_establish(struct llarp_link *l, struct llarp_link_establish_job *job)
  {
    server *link = static_cast< server * >(l->impl);
    {
      llarp::Addr dst = job->ai;
      printf("try establish to %s\n", dst.to_string().c_str());
      if(link->has_session_to(dst))
      {
        printf("already have session\n");
        return false;
      }

      session *s = link->create_session(dst);

      link->put_session(dst, s);

      s->establish_job = job;
      s->introduce(job->ai.enc_key);
    }
    printf("introduced\n");
    return true;
  }

  void
  link_mark_session_active(struct llarp_link *link,
                           struct llarp_link_session *s)
  {
    // TODO: implement
  }

  void
  link_free(struct llarp_link *l)
  {
    server *link = static_cast< server * >(l->impl);
    delete link;
  }
}

extern "C" {

void
iwp_link_init(struct llarp_link *link, struct llarp_iwp_args args,
              struct llarp_msg_muxer *muxer)
{
  link->impl = iwp::link_alloc(args.mem, muxer, args.keyfile, args.crypto,
                               args.logic, args.cryptoworker);
  link->name = iwp::link_name;
  link->get_our_address     = iwp::link_get_addr;
  link->configure           = iwp::link_configure;
  link->start_link          = iwp::link_start;
  link->stop_link           = iwp::link_stop;
  link->iter_sessions       = iwp::link_iter_sessions;
  link->try_establish       = iwp::link_try_establish;
  link->mark_session_active = iwp::link_mark_session_active;
  link->free_impl           = iwp::link_free;
}
}
