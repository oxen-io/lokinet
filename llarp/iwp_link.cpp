#include <llarp/crypto_async.h>
#include <llarp/iwp.h>
#include <llarp/net.h>
#include <llarp/time.h>
#include <llarp/frame_handler.hpp>

#include <sodium/crypto_sign_ed25519.h>

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
    byte_t *ptr;

    frame_header(byte_t *buf) : ptr(buf)
    {
    }

    byte_t *
    data()
    {
      return ptr + 6;
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

    uint16_t
    size() const
    {
      uint16_t sz = (ptr[3] | 0x00ff) << 8;
      sz |= (ptr[2]  & 0x00ff);
      return sz;
    }

    void
    setsize(uint16_t sz)
    {
      ptr[3] = (sz | 0xff00) >> 8;
      ptr[2] = (sz | 0x00ff);
    }

    uint8_t
    flags() const
    {
      return ptr[5];
    }

    void
    setflag(header_flag f)
    {
      ptr[5] |= f;
    }
  };

  /** xmit header */
  struct xmit
  {
    byte_t buffer[48];

    xmit()
    {
    }

    xmit(byte_t *ptr)
    {
      memcpy(buffer, ptr, sizeof(buffer));
    }

    xmit(const xmit &other)
    {
      memcpy(buffer, other.buffer, sizeof(buffer));
    }

    void
    set_info(const byte_t *hash, uint64_t id, uint16_t fragsz, uint16_t lastsz,
             uint8_t numfrags, uint8_t flags = 0x01)
    {
      // big endian assumed
      // TODO: implement little endian
      memcpy(buffer, hash, 32);
      memcpy(buffer + 32, &id, 8);
      memcpy(buffer + 40, &fragsz, 2);
      memcpy(buffer + 42, &lastsz, 2);
      buffer[44] = 0;
      buffer[45] = 0;
      buffer[46] = numfrags;
      buffer[47] = flags;
    }

    uint64_t
    msgid() const
    {
      // big endian assumed
      // TODO: implement little endian
      const byte_t *start   = buffer + 32;
      const uint64_t *msgid = (const uint64_t *)start;
      return *msgid;
    }

    // size of each full fragment
    uint16_t
    fragsize() const
    {
      // big endian assumed
      // TODO: implement little endian
      const byte_t *start    = buffer + 40;
      const uint16_t *fragsz = (uint16_t *)start;
      return *fragsz;
    }

    // number of full fragments
    uint8_t
    numfrags() const
    {
      return buffer[46];
    }

    // size of the entire message
    size_t
    totalsize() const
    {
      return (fragsize() * numfrags()) + lastfrag();
    }

    // size of the last fragment
    uint16_t
    lastfrag() const
    {
      // big endian assumed
      // TODO: implement little endian
      const byte_t *start    = buffer + 42;
      const uint16_t *lastsz = (uint16_t *)start;
      return *lastsz;
    }

    uint8_t
    flags()
    {
      return buffer[47];
    }
  };

  typedef std::vector< uint8_t > fragment_t;

  // forward declare
  struct session;

  struct transit_message
  {
    session *parent = nullptr;
    xmit msginfo;
    std::bitset< 16 > status;

    std::map< uint16_t, fragment_t > frags;
    fragment_t lastfrag;

    transit_message()
    {
    }

    // inbound
    transit_message(const xmit &x) : msginfo(x)
    {
    }

    // outbound
    transit_message(session *s) : parent(s)
    {
    }

    bool reassemble(std::vector<byte_t> & buffer)
    {
      // TODO: implement
      return false;
    }
    
    void
    put_message(llarp_buffer_t &buf, const byte_t *hash, uint64_t id,
                uint16_t mtu = 1024)
    {
      status.reset();
      uint16_t fragid   = 0;
      uint16_t fragsize = mtu;
      while((buf.cur - buf.base) > fragsize)
      {
        fragment_t frag(fragsize);
        memcpy(frag.data(), buf.cur, fragsize);
        buf.cur += fragsize;
        frags[fragid++] = frag;
      }
      uint16_t lastfrag = buf.cur - buf.base;
      // set info for xmit
      msginfo.set_info(hash, id, mtu, frags.size(), lastfrag);
      // copy message hash
      memcpy(msginfo.buffer, hash, 32);
      put_lastfrag(buf.cur, buf.cur - buf.base);
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
    uint64_t rxids         = 0;
    uint64_t txids         = 0;
    llarp_time_t lastEvent = 0;
    std::map< uint64_t, transit_message > rx;
    std::map< uint64_t, transit_message * > tx;

    typedef std::vector< byte_t > sendbuf_t;
    std::queue< sendbuf_t > sendqueue;

    llarp::FrameHandler * handler = nullptr;

    void
    inbound_frame_complete(uint64_t id)
    {
      std::vector<byte_t> buf;
      if(rx[id].reassemble(buf) && handler)
      {
        if(handler->Process(buf))
          printf("processed frame %ld\n", id);
        else
          printf("failed to process frame %ld", id);
      }
      rx.erase(id);
    }
    
    void
    init_sendbuf(sendbuf_t &buf, msgtype t, uint16_t sz, uint8_t flags)
    {
      buf.resize(6 + sz);
      buf[0] = 0;
      buf[1] = t;
      buf[2] = sz & 0x00ff;
      buf[3] = (sz & 0xff00) >> 8;
      buf[4] = 0;
      buf[5] = flags;
    }

    void
    push_ackfor(uint64_t id, uint32_t bitmask)
    {
      sendbuf_t buf;
      // TODO: set flags to nonzero as needed
      init_sendbuf(buf, eACKS, 12, 0);
      // TODO: this assumes big endian
      memcpy(buf.data() + 6, &id, 8);
      memcpy(buf.data() + 14, &bitmask, 4);
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

      const auto bufsz = sizeof(x.buffer);

      if(sz - bufsz < x.lastfrag())
      {
        // bad size of last fragment
        printf("XMIT frag size missmatch, %ld < %d\n", sz - bufsz,
               x.lastfrag());
        return false;
      }

      // check LSB set on flags
      if(x.flags() & 0x01)
      {
        if(x.numfrags() > 0)
        {
          auto itr = rx.try_emplace(x.msgid(), x);
          if(itr.second)
          {
            // inserted, put last fragment
            itr.first->second.put_lastfrag(hdr.data() + sizeof(x.buffer),
                                           x.lastfrag());
            return true;
          }
          else
            printf("duplicate XMIT msgid=%ld\n", x.msgid());
        }
        else
        {
          // short XMIT , no fragments so just ack
          auto id = x.msgid();
          push_ackfor(id, 0); // TODO: should this be before or after handling frame?
          inbound_frame_complete(id);
        }
      }
      else
        printf("XMIT flags LSB not set\n");
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
    queue_tx(uint16_t id, transit_message *msg)
    {
      tx.try_emplace(id, msg);
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
    llarp_rc *our_router;

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
      eIntroRecv,
      eIntroSent,
      eIntroAckSent,
      eIntroAckRecv,
      eEstablished,
      eTimeout
    };

    State state;

    session(llarp_alloc *m, llarp_msg_muxer *mux, llarp_udp_io *u,
            llarp_async_iwp *i, llarp_crypto *c, llarp_logic *l,
            const byte_t *seckey, const llarp::Addr &a)
        : mem(m)
        , muxer(mux)
        , udp(u)
        , crypto(c)
        , iwp(i)
        , logic(l)
        , addr(a)
        , state(eInitial)
    {
      if(seckey)
        memcpy(eph_seckey, seckey, sizeof(llarp_seckey_t));
      else
      {
        c->encryption_keygen(eph_seckey);
      }
    }

    ~session()
    {
    }

    static bool
    sendto(llarp_link_session *s, llarp_buffer_t msg)
    {
      session *self      = static_cast< session * >(s->impl);
      transit_message *m = new transit_message(self);
      auto id            = self->frame.txids++;
      llarp_hash_t digest;
      self->crypto->hash(digest, msg);
      m->put_message(msg, digest, id);
      self->add_outbound_message(id, m);
      return true;
    }

    void
    add_outbound_message(uint16_t id, transit_message *msg)
    {
      frame.queue_tx(id, msg);
      pump();
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
        case eInitial:
          // got intro
          on_intro(buf, sz);
          return;
        case eIntroSent:
          // got intro ack
          on_intro_ack(buf, sz);
          return;
        case eIntroAckSent:
          // probably a session start
          on_session_start(buf, sz);
          return;
        case eEstablished:
          // session is started
          decrypt_frame(buf, sz);
        default:
          // invalid state?
          return;
      }
    }

    static void
    handle_verify_session_start(iwp_async_session_start *s)
    {
      session *self = static_cast< session * >(s->user);
      if(!s->buf)
      {
        // verify fail
        // TODO: remove session?
        printf("session start verify fail\n");
        return;
      }
      printf("session start okay\n");

      // auto msg = new transit_message;

      // auto buffer = llarp::EncodeLIM< decltype(buf) >(buf, our_router);
    }

    void
    on_session_start(const void *buf, size_t sz)
    {
      // own the buffer
      memcpy(workbuf, buf, sz);
      // verify session start
      start.buf           = workbuf;
      start.sz            = sz;
      start.nonce         = workbuf + 32;
      start.token         = token;
      start.remote_pubkey = remote;
      start.secretkey     = eph_seckey;
      start.sessionkey    = sessionkey;
      start.user          = this;
      start.hook          = &handle_verify_session_start;
      iwp_call_async_verify_session_start(iwp, &start);
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
      start.remote_pubkey = remote;
      start.secretkey     = eph_seckey;
      start.sessionkey    = sessionkey;
      start.user          = this;
      start.hook          = &handle_generated_session_start;
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

      delete frame;
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
      delete frame;
    }

    iwp_async_frame *
    alloc_frame(const void *buf, size_t sz)
    {
      iwp_async_frame *frame = new iwp_async_frame;
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

    static void
    handle_verify_intro(iwp_async_intro *intro)
    {
      session *self = static_cast< session * >(intro->user);
      if(!intro->buf)
      {
        printf("verify intro fail\n");
        // TODO: delete session from parent here
        return;
      }
      self->intro_ack();
    }

    static void
    handle_introack_generated(iwp_async_introack *i)
    {
      session *link = static_cast< session * >(i->user);
      if(i->buf)
      {
        printf("sending introack...\n");
        llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz);
        link->EnterState(eIntroAckSent);
      }
      else
      {
        // failed to generate?
      }
    }

    void
    intro_ack()
    {
      uint16_t w1sz = rand() % 32;
      introack.buf  = workbuf;
      introack.sz   = (32 * 3) + w1sz;
      // randomize padding
      if(w1sz)
        crypto->randbytes(introack.buf + (32 * 3), w1sz);

      // randomize nonce
      introack.nonce = introack.buf + 32;
      crypto->randbytes(introack.nonce, 32);
      // randomize token
      introack.token = token;
      crypto->randbytes(introack.token, 32);

      // keys
      introack.remote_pubkey = remote;
      introack.secretkey     = eph_seckey;

      // call
      introack.user = this;
      introack.hook = &handle_introack_generated;
      iwp_call_async_gen_introack(iwp, &introack);
    }

    void
    on_intro(const void *buf, size_t sz)
    {
      printf("iwp intro\n");
      if(sz >= sizeof(workbuf))
      {
        // too big?
        printf("intro too big\n");
        // TOOD: session destroy ?
        return;
      }
      // copy so we own it
      memcpy(workbuf, buf, sz);
      intro.buf = workbuf;
      intro.sz  = sz;
      // give secret key
      intro.secretkey = eph_seckey;
      // and nonce
      intro.nonce = intro.buf + 32;
      intro.user  = this;
      // set call back hook
      intro.hook = &handle_verify_intro;
      // put remote pubkey into this buffer
      intro.remote_pubkey = remote;

      // call
      EnterState(eIntroRecv);
      iwp_call_async_verify_intro(iwp, &intro);
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
      introack.token         = token;
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
        printf("sending intro...\n");
        llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz);
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

      intro.nonce     = intro.buf + 32;
      intro.secretkey = eph_seckey;
      // copy in pubkey
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
    llarp::Addr addr;
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

    ~server()
    {
      llarp_async_iwp_free(iwp);
    }

    session *
    create_session(const llarp::Addr &src, const byte_t *seckey)
    {
      return new session(mem, muxer, &udp, iwp, crypto, logic, seckey, src);
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
      s.impl               = impl;
      s.sendto             = &session::sendto;
      s.timeout            = &session::is_timedout;
      s.close              = &session::close;
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
          s   = create_session(src, seckey);
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
    clear_sessions()
    {
      lock_t lock(m_sessions_Mutex);
      auto itr = m_sessions.begin();
      while(itr != m_sessions.end())
      {
        session *s = static_cast< session * >(itr->second.impl);
        delete s;
        itr = m_sessions.erase(itr);
      }
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
            printf("remove session for %s\n", addr.to_string().c_str());
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
      printf("ensure transport private key at %s\n", keyfile);
      std::error_code ec;
      if(!fs::exists(keyfile, ec))
      {
        if(!keygen(keyfile))
          return false;
      }
      std::ifstream f(keyfile);
      if(f.is_open())
      {
        f.read((char *)seckey, sizeof(llarp_seckey_t));
        return true;
      }
      return false;
    }

    bool
    keygen(const char *fname)
    {
      crypto->encryption_keygen(seckey);
      printf("transport key generated\n");
      std::ofstream f(fname);
      if(f.is_open())
      {
        f.write((char *)seckey, sizeof(llarp_seckey_t));
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
      server *link = static_cast< server * >(udp->user);
      llarp::Addr addr(*saddr);
      printf("got %ld from %s\n", sz, addr.to_string().c_str());
      session *s = link->ensure_session(addr);
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
    llarp::Zero(link->keyfile, sizeof(link->keyfile));
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
    addr->rank   = 1;
    strncpy(addr->dialect, link_name(), sizeof(addr->dialect));
    memcpy(addr->enc_key, link->pubkey(), 32);
    memcpy(addr->ip.s6_addr, link->addr.addr6(), 16);
    addr->port = link->addr.port();
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
    sockaddr_in ip4addr;
    sockaddr_in6 ip6addr;
    sockaddr *addr = nullptr;
    switch(af)
    {
      case AF_INET:
        addr             = (sockaddr *)&ip4addr;
        ip4addr.sin_port = htons(port);
        break;
      case AF_INET6:
        addr              = (sockaddr *)&ip6addr;
        ip6addr.sin6_port = htons(port);
        break;
        // TODO: AF_PACKET
      default:
        return false;
    }

    if(!llarp_getifaddr(ifname, af, addr))
    {
      printf("failed to get address for %s\n", ifname);
      return false;
    }

    switch(af)
    {
      case AF_INET:
        ip4addr.sin_port = htons(port);
        break;
      case AF_INET6:
        ip6addr.sin6_port = htons(port);
        break;
        // TODO: AF_PACKET
      default:
        return false;
    }

    link->addr = *addr;
    printf("bind to %s at %s\n", ifname, link->addr.to_string().c_str());
    link->netloop      = netloop;
    link->udp.recvfrom = &server::handle_recvfrom;
    link->udp.user     = link;
    return llarp_ev_add_udp(link->netloop, &link->udp, link->addr) != -1;
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
      llarp::Addr dst(job->ai);
      printf("try establish to %s\n", dst.to_string().c_str());
      if(link->has_session_to(dst))
      {
        printf("already have session\n");
        return false;
      }
      session *s = link->create_session(dst, nullptr);

      link->put_session(dst, s);

      s->establish_job = job;
      s->introduce(job->ai.enc_key);
    }
    return true;
  }

  void
  link_mark_session_active(struct llarp_link *link,
                           struct llarp_link_session *s)
  {
    static_cast< session * >(s->impl)->frame.alive();
  }

  void
  link_free(struct llarp_link *l)
  {
    server *link = static_cast< server * >(l->impl);
    llarp_ev_close_udp(&link->udp);
    link->clear_sessions();
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
