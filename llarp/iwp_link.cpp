#include <llarp/crypto_async.h>
#include <llarp/iwp.h>
#include <llarp/net.h>
#include <llarp/time.h>
#include <llarp/crypto.hpp>
#include "address_info.hpp"
#include "link/encoder.hpp"

#include <sodium/crypto_sign_ed25519.h>

#include <algorithm>
#include <bitset>
#include <cassert>
#include <fstream>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

#include "buffer.hpp"
#include "fs.hpp"
#include "logger.hpp"
#include "mem.hpp"
#include "net.hpp"
#include "router.hpp"
#include "str.hpp"

namespace iwp
{
  // session activity timeout is 5s
  constexpr llarp_time_t SESSION_TIMEOUT = 5000;

  constexpr size_t MAX_PAD = 128;

  enum msgtype
  {
    eALIV = 0x00,
    eXMIT = 0x01,
    eACKS = 0x02,
    eFRAG = 0x03
  };

  struct sendbuf_t
  {
    sendbuf_t(size_t s) : sz(s)
    {
      buf = new byte_t[s];
    }

    ~sendbuf_t()
    {
      delete[] buf;
    }

    byte_t *buf;
    size_t sz;

    size_t
    size() const
    {
      return sz;
    }

    byte_t *
    data()
    {
      return buf;
    }
  };

  enum header_flag
  {
    eSessionInvalidated = (1 << 0),
    eHighPacketDrop     = (1 << 1),
    eHighMTUDetected    = (1 << 2),
    eProtoUpgrade       = (1 << 3)
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
      uint16_t sz;
      memcpy(&sz, ptr + 2, 2);
      return sz;
    }

    void
    setsize(uint16_t sz)
    {
      memcpy(ptr + 2, &sz, 2);
    }

    uint8_t &
    flags()
    {
      return ptr[5];
    }

    void
    setflag(header_flag f)
    {
      ptr[5] |= f;
    }
  };

  byte_t *
  init_sendbuf(sendbuf_t *buf, msgtype t, uint16_t sz, uint8_t flags)
  {
    frame_header hdr(buf->data());
    hdr.version() = 0;
    hdr.msgtype() = t;
    hdr.setsize(sz);
    buf->data()[4] = 0;
    buf->data()[5] = flags;
    return hdr.data();
  }

  /** xmit header */
  struct xmit
  {
    byte_t buffer[48];

    xmit() = default;

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

    const byte_t *
    hash() const
    {
      return &buffer[0];
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

  // forward declare
  struct session;
  struct server;

  struct transit_message
  {
    xmit msginfo;
    std::bitset< 32 > status = {};

    typedef std::vector< byte_t > fragment_t;

    std::unordered_map< byte_t, fragment_t > frags;
    fragment_t lastfrag;

    void
    clear()
    {
      frags.clear();
      lastfrag.clear();
    }

    // calculate acked bitmask
    uint32_t
    get_bitmask() const
    {
      uint32_t bitmask = 0;
      uint8_t idx      = 0;
      while(idx < 32)
      {
        bitmask |= (status.test(idx) ? (1 << idx) : 0);
        ++idx;
      }
      return bitmask;
    }

    // outbound
    transit_message(llarp_buffer_t buf, const byte_t *hash, uint64_t id,
                    uint16_t mtu = 1024)
    {
      put_message(buf, hash, id, mtu);
    }

    // inbound
    transit_message(const xmit &x) : msginfo(x)
    {
      byte_t fragidx    = 0;
      uint16_t fragsize = x.fragsize();
      while(fragidx < x.numfrags())
      {
        frags[fragidx].resize(fragsize);
        ++fragidx;
      }
      status.reset();
    }

    /// ack packets based off a bitmask
    void
    ack(uint32_t bitmask)
    {
      uint8_t idx = 0;
      while(idx < 32)
      {
        if(bitmask & (1 << idx))
        {
          status.set(idx);
        }
        ++idx;
      }
    }

    bool
    should_send_ack() const
    {
      if(msginfo.numfrags() == 0)
        return true;
      return status.count() % (1 + (msginfo.numfrags() / 2)) == 0;
    }

    bool
    completed() const
    {
      for(byte_t idx = 0; idx < msginfo.numfrags(); ++idx)
      {
        if(!status.test(idx))
          return false;
      }
      return true;
    }

    template < typename T >
    void
    generate_xmit(T &queue, byte_t flags = 0)
    {
      uint16_t sz = lastfrag.size() + sizeof(msginfo.buffer);
      queue.push(new sendbuf_t(sz + 6));
      auto body_ptr = init_sendbuf(queue.back(), eXMIT, sz, flags);
      memcpy(body_ptr, msginfo.buffer, sizeof(msginfo.buffer));
      body_ptr += sizeof(msginfo.buffer);
      memcpy(body_ptr, lastfrag.data(), lastfrag.size());
    }

    template < typename T >
    void
    retransmit_frags(T &queue, byte_t flags = 0)
    {
      auto msgid    = msginfo.msgid();
      auto fragsize = msginfo.fragsize();
      for(auto &frag : frags)
      {
        if(status.test(frag.first))
          continue;
        uint16_t sz = 9 + fragsize;
        queue.push(new sendbuf_t(sz + 6));
        auto body_ptr = init_sendbuf(queue.back(), eFRAG, sz, flags);
        // TODO: assumes big endian
        memcpy(body_ptr, &msgid, 8);
        body_ptr[8] = frag.first;
        memcpy(body_ptr + 9, frag.second.data(), fragsize);
      }
    }

    bool
    reassemble(std::vector< byte_t > &buffer)
    {
      auto total = msginfo.totalsize();
      buffer.resize(total);
      auto fragsz = msginfo.fragsize();
      auto ptr    = &buffer[0];
      for(byte_t idx = 0; idx < msginfo.numfrags(); ++idx)
      {
        if(!status.test(idx))
          return false;
        memcpy(ptr, frags[idx].data(), fragsz);
        ptr += fragsz;
      }
      memcpy(ptr, lastfrag.data(), lastfrag.size());
      return true;
    }

    void
    put_message(llarp_buffer_t buf, const byte_t *hash, uint64_t id,
                uint16_t mtu = 1024)
    {
      status.reset();
      uint8_t fragid    = 0;
      uint16_t fragsize = mtu;
      size_t left       = buf.sz;
      while(left > fragsize)
      {
        auto &frag = frags[fragid];
        frag.resize(fragsize);
        memcpy(frag.data(), buf.cur, fragsize);
        buf.cur += fragsize;
        fragid++;
        left -= fragsize;
      }
      uint16_t lastfrag = buf.sz - (buf.cur - buf.base);
      // set info for xmit
      msginfo.set_info(hash, id, fragsize, lastfrag, fragid);
      put_lastfrag(buf.cur, lastfrag);
    }

    void
    put_lastfrag(byte_t *buf, size_t sz)
    {
      lastfrag.resize(sz);
      memcpy(lastfrag.data(), buf, sz);
    }

    bool
    put_frag(byte_t fragno, byte_t *buf)
    {
      auto itr = frags.find(fragno);
      if(itr == frags.end())
        return false;
      memcpy(itr->second.data(), buf, msginfo.fragsize());
      status.set(fragno);
      return true;
    }
  };

  struct frame_state
  {
    byte_t rxflags         = 0;
    byte_t txflags         = 0;
    uint64_t rxids         = 0;
    uint64_t txids         = 0;
    llarp_time_t lastEvent = 0;
    std::unordered_map< uint64_t, transit_message * > rx;
    std::unordered_map< uint64_t, transit_message * > tx;

    typedef std::queue< sendbuf_t * > sendqueue_t;

    llarp_router *router       = nullptr;
    llarp_link_session *parent = nullptr;

    sendqueue_t sendqueue;

    /// return true if both sides have the same state flags
    bool
    flags_agree(byte_t flags) const
    {
      return ((rxflags & flags) & (txflags & flags)) == flags;
    }

    void
    clear()
    {
      auto _rx = rx;
      auto _tx = tx;
      for(auto &item : _rx)
        delete item.second;
      for(auto &item : _tx)
        delete item.second;
      rx.clear();
      tx.clear();
    }

    bool
    inbound_frame_complete(uint64_t id);

    void
    push_ackfor(uint64_t id, uint32_t bitmask)
    {
      llarp::Debug("ACK for msgid=", id, " mask=", bitmask);
      sendqueue.push(new sendbuf_t(12 + 6));
      auto body_ptr = init_sendbuf(sendqueue.back(), eACKS, 12, txflags);
      // TODO: this assumes big endian
      memcpy(body_ptr, &id, 8);
      memcpy(body_ptr + 8, &bitmask, 4);
    }

    bool
    got_xmit(frame_header hdr, size_t sz)
    {
      if(hdr.size() > sz)
      {
        // overflow
        llarp::Warn("invalid XMIT frame size ", hdr.size(), " > ", sz);
        return false;
      }
      sz = hdr.size();

      // extract xmit data
      xmit x(hdr.data());

      const auto bufsz = sizeof(x.buffer);

      if(sz - bufsz < x.lastfrag())
      {
        // bad size of last fragment
        llarp::Warn("XMIT frag size missmatch ", sz - bufsz, " < ",
                    x.lastfrag());
        return false;
      }

      // check LSB set on flags
      if(x.flags() & 0x01)
      {
        auto id  = x.msgid();
        auto itr = rx.find(id);
        if(itr == rx.end())
        {
          auto msg = new transit_message(x);
          rx[id]   = msg;
          llarp::Debug("got message XMIT with ", (int)x.numfrags(),
                       " fragments");
          // inserted, put last fragment
          msg->put_lastfrag(hdr.data() + sizeof(x.buffer), x.lastfrag());
          push_ackfor(id, 0);
          if(x.numfrags() == 0)
          {
            return inbound_frame_complete(id);
          }
          return true;
        }
        else
          llarp::Warn("duplicate XMIT msgid=", x.msgid());
      }
      else
        llarp::Warn("LSB not set on flags");
      return false;
    }

    void
    alive()
    {
      lastEvent = llarp_time_now_ms();
    }

    bool
    got_frag(frame_header hdr, size_t sz)
    {
      if(hdr.size() > sz)
      {
        // overflow
        llarp::Warn("invalid FRAG frame size ", hdr.size(), " > ", sz);
        return false;
      }
      sz = hdr.size();

      if(sz <= 9)
      {
        // underflow
        llarp::Warn("invalid FRAG frame size ", sz, " <= 9");
        return false;
      }

      uint64_t msgid;
      byte_t fragno;
      // assumes big endian
      // TODO: implement little endian
      memcpy(&msgid, hdr.data(), 8);
      memcpy(&fragno, hdr.data() + 8, 1);

      auto itr = rx.find(msgid);
      if(itr == rx.end())
      {
        llarp::Warn("no such RX fragment, msgid=", msgid);
        return false;
      }
      auto fragsize = itr->second->msginfo.fragsize();
      if(fragsize != sz - 9)
      {
        llarp::Warn("RX fragment size missmatch ", fragsize, " != ", sz - 9);
        return false;
      }
      llarp::Debug("RX got fragment ", (int)fragno, " msgid=", msgid);
      if(!itr->second->put_frag(fragno, hdr.data() + 9))
      {
        llarp::Warn("inbound message does not have fragment msgid=", msgid,
                    " fragno=", (int)fragno);
        return false;
      }
      auto mask = itr->second->get_bitmask();
      if(itr->second->completed())
      {
        push_ackfor(msgid, mask);
        return inbound_frame_complete(msgid);
      }
      else if(itr->second->should_send_ack())
      {
        push_ackfor(msgid, mask);
      }
      return true;
    }

    bool
    got_acks(frame_header hdr, size_t sz);

    // queue new outbound message
    void
    queue_tx(uint64_t id, transit_message *msg)
    {
      tx[id] = msg;
      msg->generate_xmit(sendqueue, txflags);
    }

    void
    retransmit()
    {
      for(auto &item : tx)
      {
        item.second->retransmit_frags(sendqueue, txflags);
      }
    }

    // get next frame to encrypt and transmit
    bool
    next_frame(llarp_buffer_t *buf)
    {
      auto left = sendqueue.size();
      llarp::Debug("next frame, ", left, " frames left in send queue");
      if(left)
      {
        sendbuf_t *send = sendqueue.front();
        buf->base       = send->data();
        buf->cur        = send->data();
        buf->sz         = send->size();
        return true;
      }
      return false;
    }

    void
    pop_next_frame()
    {
      sendbuf_t *buf = sendqueue.front();
      sendqueue.pop();
      delete buf;
    }

    bool
    process(byte_t *buf, size_t sz)
    {
      frame_header hdr(buf);
      if(hdr.flags() & eSessionInvalidated)
      {
        rxflags |= eSessionInvalidated;
      }
      switch(hdr.msgtype())
      {
        case eALIV:
          if(rxflags & eSessionInvalidated)
          {
            txflags |= eSessionInvalidated;
          }
          return true;
        case eXMIT:
          return got_xmit(hdr, sz - 6);
        case eACKS:
          return got_acks(hdr, sz - 6);
        case eFRAG:
          return got_frag(hdr, sz - 6);
        default:
          return false;
      }
    }
  };

  struct session
  {
    llarp_udp_io *udp;
    llarp_crypto *crypto;
    llarp_async_iwp *iwp;
    llarp_logic *logic;

    llarp_link_session *parent = nullptr;
    server *serv               = nullptr;

    llarp_rc *our_router = nullptr;
    llarp_rc remote_router;

    llarp::SecretKey eph_seckey;
    llarp::PubKey remote;
    llarp::SharedSecret sessionkey;

    llarp_link_establish_job *establish_job = nullptr;

    uint32_t establish_job_id = 0;
    uint32_t frames           = 0;

    llarp::Addr addr;
    iwp_async_intro intro;
    iwp_async_introack introack;
    iwp_async_session_start start;
    frame_state frame;

    byte_t token[32];
    byte_t workbuf[MAX_PAD + 128];

    enum State
    {
      eInitial,
      eIntroRecv,
      eIntroSent,
      eIntroAckSent,
      eIntroAckRecv,
      eSessionStartSent,
      eLIMSent,
      eEstablished,
      eTimeout
    };

    State state;

    session(llarp_udp_io *u, llarp_async_iwp *i, llarp_crypto *c,
            llarp_logic *l, const byte_t *seckey, const llarp::Addr &a)
        : udp(u), crypto(c), iwp(i), logic(l), addr(a), state(eInitial)
    {
      eph_seckey = seckey;
      llarp::Zero(&remote_router, sizeof(llarp_rc));
      crypto->randbytes(token, 32);
    }

    ~session()
    {
      llarp_rc_free(&remote_router);
      frame.clear();
    }

    static llarp_rc *
    get_remote_router(llarp_link_session *s)
    {
      session *self = static_cast< session * >(s->impl);
      return &self->remote_router;
    }

    static bool
    sendto(llarp_link_session *s, llarp_buffer_t msg)
    {
      session *self = static_cast< session * >(s->impl);
      auto id       = self->frame.txids++;
      llarp::ShortHash digest;
      self->crypto->shorthash(digest, msg);
      transit_message *m = new transit_message(msg, digest, id);
      self->add_outbound_message(id, m);
      return true;
    }

    void
    add_outbound_message(uint64_t id, transit_message *msg)
    {
      llarp::Debug("add outbound message ", id, " of size ",
                   msg->msginfo.totalsize(),
                   " numfrags=", (int)msg->msginfo.numfrags(),
                   " lastfrag=", (int)msg->msginfo.lastfrag());
      frame.queue_tx(id, msg);
      pump();
    }

    static void
    handle_invalidate_timer(void *user);

    bool
    CheckRCValid()
    {
      // verify signatuire
      if(!llarp_rc_verify_sig(crypto, &remote_router))
        return false;

      auto &list = remote_router.addrs->list;
      if(list.size() == 0)  // the remote node is a client node so accept it
        return true;
      // check if the RC owns a pubkey that we are using
      for(auto &ai : list)
      {
        if(memcmp(ai.enc_key, remote, PUBKEYSIZE) == 0)
          return true;
      }
      return false;
    }

    void
    pump()
    {
      llarp_buffer_t buf;
      while(frame.next_frame(&buf))
      {
        encrypt_frame_async_send(buf.base, buf.sz);
        frame.pop_next_frame();
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

        case eSessionStartSent:
        case eLIMSent:
        case eEstablished:
          // session is started
          decrypt_frame(buf, sz);
        default:
          // invalid state?
          return;
      }
    }

    static void
    handle_verify_session_start(iwp_async_session_start *s);

    void
    send_LIM()
    {
      llarp::Debug("send LIM");
      llarp::ShortHash digest;
      // 64 bytes overhead for link message
      byte_t tmp[MAX_RC_SIZE + 64];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      // return a llarp_buffer_t of encoded link message
      if(llarp::EncodeLIM(&buf, our_router))
      {
        // rewind message buffer
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        // hash message buffer
        crypto->shorthash(digest, buf);
        auto id  = frame.txids++;
        auto msg = new transit_message(buf, digest, id);
        // put into outbound send queue
        add_outbound_message(id, msg);
        EnterState(eLIMSent);
      }
      else
        llarp::Error("LIM Encode failed");
    }

    static void
    send_keepalive(void *user);

    // return true if we should be removed
    bool
    Tick(uint64_t now)
    {
      if(timedout(now, SESSION_TIMEOUT))
      {
        // we are timed out
        // when we are done doing stuff with all of our frames from the crypto
        // workers we are done
        llarp::Debug(addr, " timed out with ", frames, " frames left");
        return frames == 0;
      }
      if(is_invalidated())
      {
        // both sides agreeed to session invalidation
        // terminate our session when all of our frames from the crypto workers
        // are done
        llarp::Debug(addr, " invaldiated session with ", frames,
                     " frames left");
        return frames == 0;
      }
      // send keepalive if we are established or a session is made
      if(state == eEstablished || state == eLIMSent)
      {
        send_keepalive(this);
        frame.retransmit();
        pump();
      }

      // TODO: determine if we are too idle
      return false;
    }

    void
    session_established()
    {
      EnterState(eEstablished);
      llarp_logic_cancel_call(logic, establish_job_id);
    }

    void
    on_session_start(const void *buf, size_t sz)
    {
      if(sz > sizeof(workbuf))
      {
        llarp::Debug("session start too big");
        return;
      }
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
      auto diff = now - frame.lastEvent;
      return diff >= timeout;
    }

    static bool
    is_timedout(llarp_link_session *s)
    {
      auto now = llarp_time_now_ms();
      return static_cast< session * >(s->impl)->timedout(now);
    }

    static void
    handle_session_established(void *user)
    {
      session *impl = static_cast< session * >(user);
      impl->session_established();
    }

    static void
    set_established(llarp_link_session *s)
    {
      session *impl = static_cast< session * >(s->impl);
      llarp_logic_queue_job(impl->logic, {impl, &handle_session_established});
    }

    static void
    close(llarp_link_session *s)
    {
      session *impl = static_cast< session * >(s->impl);
      // set our side invalidated and close async when the other side also marks
      // as session invalidated
      impl->frame.txflags |= eSessionInvalidated;
      // TODO: add timer for session invalidation
      llarp_logic_queue_job(impl->logic, {impl, &send_keepalive});
    }

    static void
    handle_verify_introack(iwp_async_introack *introack)
    {
      session *link = static_cast< session * >(introack->user);
      if(introack->buf == nullptr)
      {
        // invalid signature
        llarp::Error("introack verify failed from ", link->addr);
        link->done();
        return;
      }
      link->EnterState(eIntroAckRecv);
      link->session_start();
    }

    static void
    handle_generated_session_start(iwp_async_session_start *start)
    {
      session *link = static_cast< session * >(start->user);
      if(llarp_ev_udp_sendto(link->udp, link->addr, start->buf, start->sz)
         == -1)
        llarp::Error("sendto failed");
      link->EnterState(eSessionStartSent);
    }

    bool
    is_invalidated() const
    {
      return frame.flags_agree(eSessionInvalidated);
    }

    void
    session_start()
    {
      size_t w2sz = rand() % MAX_PAD;
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
      llarp::Debug("rx ", frame->sz, " frames=", self->frames);
      self->frames--;
      if(frame->success)
      {
        if(self->frame.process(frame->buf + 64, frame->sz - 64))
        {
          self->frame.alive();
          self->pump();
        }
        else
          llarp::Error("invalid frame");
      }
      else
        llarp::Error("decrypt frame fail");
    }

    void
    decrypt_frame(const void *buf, size_t sz)
    {
      if(sz > 64)
      {
        iwp_async_frame *frame = alloc_frame(buf, sz);
        frame->hook            = &handle_frame_decrypt;
        iwp_call_async_frame_decrypt(iwp, frame);
      }
      else
        llarp::Warn("short packet of ", sz, " bytes");
    }

    static void
    handle_frame_encrypt(iwp_async_frame *frame)
    {
      session *self = static_cast< session * >(frame->user);
      llarp::Debug("tx ", frame->sz, " frames=", self->frames);
      if(llarp_ev_udp_sendto(self->udp, self->addr, frame->buf, frame->sz)
         == -1)
        llarp::Warn("sendto failed");
      self->frames--;
    }

    iwp_async_frame *
    alloc_frame(const void *buf, size_t sz)
    {
      // TODO don't hard code 1500
      if(sz > 1500)
        return nullptr;

      iwp_async_frame *frame = new iwp_async_frame();
      if(buf)
        memcpy(frame->buf, buf, sz);
      frame->sz         = sz;
      frame->user       = this;
      frame->sessionkey = sessionkey;
      frames++;
      return frame;
    }

    void
    encrypt_frame_async_send(const void *buf, size_t sz)
    {
      // 64 bytes frame overhead for nonce and hmac
      iwp_async_frame *frame = alloc_frame(nullptr, sz + 64);
      memcpy(frame->buf + 64, buf, sz);
      auto padding = rand() % MAX_PAD;
      if(padding)
        crypto->randbytes(frame->buf + 64 + sz, padding);
      frame->sz += padding;
      frame->hook = &handle_frame_encrypt;
      iwp_call_async_frame_encrypt(iwp, frame);
    }

    static void
    handle_verify_intro(iwp_async_intro *intro);

    static void
    handle_introack_generated(iwp_async_introack *i);

    void
    intro_ack()
    {
      uint16_t w1sz = rand() % MAX_PAD;
      introack.buf  = workbuf;
      introack.sz   = (32 * 3) + w1sz;
      // randomize padding
      if(w1sz)
        crypto->randbytes(introack.buf + (32 * 3), w1sz);

      // randomize nonce
      introack.nonce = introack.buf + 32;
      crypto->randbytes(introack.nonce, 32);
      // token
      introack.token = token;

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
      if(sz >= sizeof(workbuf))
      {
        // too big?
        llarp::Error("intro too big");
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
      if(sz >= sizeof(workbuf))
      {
        // too big?
        llarp::Error("introack too big");
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

    static llarp_link *
    get_parent(llarp_link_session *s);

    static void
    handle_generated_intro(iwp_async_intro *i)
    {
      session *link = static_cast< session * >(i->user);
      if(i->buf)
      {
        llarp::Debug("send intro");
        if(llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz) == -1)
        {
          llarp::Warn("send intro failed");
          return;
        }
        link->EnterState(eIntroSent);
      }
      else
      {
        llarp::Warn("failed to generate intro");
      }
    }

    static void
    handle_establish_timeout(void *user, uint64_t orig, uint64_t left);

    void
    introduce(uint8_t *pub)
    {
      memcpy(remote, pub, 32);
      intro.buf   = workbuf;
      size_t w0sz = (rand() % MAX_PAD);
      intro.sz    = (32 * 3) + w0sz;
      // randomize w0
      if(w0sz)
      {
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
      // start introduce timer
      establish_job_id = llarp_logic_call_later(
          logic, {5000, this, &handle_establish_timeout});
    }

    // handle session being over
    // called right before deallocation
    void
    done();

    void
    EnterState(State st)
    {
      frame.alive();
      state = st;
    }
  };

  struct server
  {
    typedef std::mutex mtx_t;
    typedef std::lock_guard< mtx_t > lock_t;

    llarp_router *router;
    llarp_logic *logic;
    llarp_crypto *crypto;
    llarp_ev_loop *netloop;
    llarp_async_iwp *iwp;
    llarp_link *parent = nullptr;
    llarp_udp_io udp;
    llarp::Addr addr;
    char keyfile[255];
    uint32_t timeout_job_id;

    typedef std::unordered_map< llarp::Addr, llarp_link_session,
                                llarp::addrhash >
        LinkMap_t;

    LinkMap_t m_sessions;
    mtx_t m_sessions_Mutex;

    typedef std::unordered_map< llarp::PubKey, llarp::Addr, llarp::PubKeyHash >
        SessionMap_t;

    SessionMap_t m_Connected;
    mtx_t m_Connected_Mutex;

    llarp::SecretKey seckey;

    server(llarp_router *r, llarp_crypto *c, llarp_logic *l,
           llarp_threadpool *w)
    {
      router = r;
      crypto = c;
      logic  = l;
      iwp    = llarp_async_iwp_new(crypto, logic, w);
    }

    ~server()
    {
      llarp_async_iwp_free(iwp);
    }

    // set that src address has identity pubkey
    void
    MapAddr(const llarp::Addr &src, const llarp::PubKey &identity)
    {
      lock_t lock(m_Connected_Mutex);
      m_Connected[identity] = src;
    }

    static bool
    HasSessionToRouter(llarp_link *l, const byte_t *pubkey)
    {
      server *serv = static_cast< server * >(l->impl);
      llarp::PubKey pk(pubkey);
      lock_t lock(serv->m_Connected_Mutex);
      return serv->m_Connected.find(pk) != serv->m_Connected.end();
    }

    void
    TickSessions()
    {
      auto now = llarp_time_now_ms();
      {
        lock_t lock(m_sessions_Mutex);
        std::set< llarp::Addr > remove;
        for(auto &itr : m_sessions)
        {
          session *s = static_cast< session * >(itr.second.impl);
          if(s && s->Tick(now))
            remove.insert(itr.first);
        }

        for(const auto &addr : remove)
          RemoveSessionByAddr(addr);
      }
    }

    static bool
    SendToSession(llarp_link *l, const byte_t *pubkey, llarp_buffer_t buf)
    {
      server *serv = static_cast< server * >(l->impl);
      {
        lock_t lock(serv->m_Connected_Mutex);
        auto itr = serv->m_Connected.find(pubkey);
        if(itr != serv->m_Connected.end())
        {
          lock_t innerlock(serv->m_sessions_Mutex);
          auto inner_itr = serv->m_sessions.find(itr->second);
          if(inner_itr != serv->m_sessions.end())
          {
            llarp_link_session *link = &inner_itr->second;
            return link->sendto(link, buf);
          }
        }
      }
      return false;
    }

    void
    UnmapAddr(const llarp::Addr &src)
    {
      lock_t lock(m_Connected_Mutex);
      // std::unordered_map< llarp::pubkey, llarp::Addr, llarp::pubkeyhash >
      auto itr = std::find_if(
          m_Connected.begin(), m_Connected.end(),
          [src](const std::pair< llarp::PubKey, llarp::Addr > &item) -> bool {
            return src == item.second;
          });
      if(itr == std::end(m_Connected))
        return;

      // tell router we are done with this session
      router->SessionClosed(itr->first);

      m_Connected.erase(itr);
    }

    session *
    create_session(const llarp::Addr &src)
    {
      auto s  = new session(&udp, iwp, crypto, logic, seckey, src);
      s->serv = this;
      return s;
    }

    bool
    has_session_to(const llarp::Addr &dst)
    {
      lock_t lock(m_sessions_Mutex);
      return m_sessions.find(dst) != m_sessions.end();
    }

    session *
    find_session(const llarp::Addr &addr)
    {
      lock_t lock(m_sessions_Mutex);
      auto itr = m_sessions.find(addr);
      if(itr == m_sessions.end())
        return nullptr;
      else
        return static_cast< session * >(itr->second.impl);
    }

    void
    put_session(const llarp::Addr &src, session *impl)
    {
      llarp_link_session s = {};
      s.impl               = impl;
      s.sendto             = &session::sendto;
      s.timeout            = &session::is_timedout;
      s.close              = &session::close;
      s.get_remote_router  = &session::get_remote_router;
      s.established        = &session::set_established;
      s.get_parent         = &session::get_parent;
      {
        lock_t lock(m_sessions_Mutex);
        m_sessions[src] = s;
        impl->parent    = &m_sessions[src];
      }
      impl->frame.router = router;
      impl->frame.parent = impl->parent;
      impl->our_router   = &router->rc;
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
    RemoveSessionByAddr(const llarp::Addr &addr)
    {
      auto itr = m_sessions.find(addr);
      if(itr != m_sessions.end())
      {
        llarp::Debug("removing session ", addr);
        UnmapAddr(addr);
        session *s = static_cast< session * >(itr->second.impl);
        m_sessions.erase(itr);
        s->done();
        if(s->frames)
        {
          llarp::Warn("session has ", s->frames,
                      " left but is idle, not deallocating session so we "
                      "leak but don't die");
        }
        else
          delete s;
      }
    }

    uint8_t *
    pubkey()
    {
      return llarp::seckey_topublic(seckey);
    }

    bool
    ensure_privkey()
    {
      llarp::Debug("ensure transport private key at ", keyfile);
      std::error_code ec;
      if(!fs::exists(keyfile, ec))
      {
        if(!keygen(keyfile))
          return false;
      }
      std::ifstream f(keyfile);
      if(f.is_open())
      {
        f.read((char *)seckey.data(), seckey.size());
        return true;
      }
      return false;
    }

    bool
    keygen(const char *fname)
    {
      crypto->encryption_keygen(seckey);
      llarp::Info("new transport key generated");
      std::ofstream f(fname);
      if(f.is_open())
      {
        f.write((char *)seckey.data(), seckey.size());
        return true;
      }
      return false;
    }

    static void
    handle_cleanup_timer(void *l, uint64_t orig, uint64_t left)
    {
      if(left)
        return;
      server *link         = static_cast< server * >(l);
      link->timeout_job_id = 0;
      link->TickSessions();
      // TODO: exponential backoff for cleanup timer ?
      link->issue_cleanup_timer(orig);
    }

    // this is called in net threadpool
    static void
    handle_recvfrom(struct llarp_udp_io *udp, const struct sockaddr *saddr,
                    const void *buf, ssize_t sz)
    {
      server *link = static_cast< server * >(udp->user);

      session *s = link->find_session(*saddr);
      if(s == nullptr)
      {
        // new inbound session
        s = link->create_session(*saddr);
        llarp::Debug("new inbound session from ", s->addr);
      }
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

  bool
  frame_state::inbound_frame_complete(uint64_t id)
  {
    bool success = false;
    std::vector< byte_t > msg;
    auto rxmsg = rx[id];
    if(rxmsg->reassemble(msg))
    {
      llarp::ShortHash digest;
      auto buf = llarp::Buffer< decltype(msg) >(msg);
      router->crypto.shorthash(digest, buf);
      if(memcmp(digest, rxmsg->msginfo.hash(), 32))
      {
        llarp::Warn("message hash missmatch ",
                    llarp::AlignedBuffer< 32 >(digest),
                    " != ", llarp::AlignedBuffer< 32 >(rxmsg->msginfo.hash()));
        return false;
      }
      success = router->HandleRecvLinkMessage(parent, buf);
      if(success)
      {
        session *impl = static_cast< session * >(parent->impl);
        if(id == 0)
        {
          // send our LIM if we are an outbound session
          if(impl->state == session::eSessionStartSent)
          {
            impl->send_LIM();
          }
          if(impl->CheckRCValid())
          {
            impl->serv->MapAddr(impl->addr, impl->remote_router.pubkey);
          }
          else
          {
            llarp::PubKey k = impl->remote_router.pubkey;
            llarp::Warn("spoofed LIM from ", k);
            impl->parent->close(impl->parent);
            success = false;
          }
        }
        llarp::Info("handled message ", id);
      }
      else
        llarp::Warn("failed to handle inbound message ", id);
    }
    else
    {
      llarp::Warn("failed to reassemble message ", id);
    }
    delete rxmsg;
    rx.erase(id);
    return success;
  }

  void
  session::handle_verify_intro(iwp_async_intro *intro)
  {
    session *self = static_cast< session * >(intro->user);
    if(!intro->buf)
    {
      llarp::Error("intro verify failed from ", self->addr, " via ",
                   self->serv->addr);
      return;
    }
    self->intro_ack();
  }

  void
  session::done()
  {
    if(establish_job_id)
    {
      llarp_logic_remove_call(logic, establish_job_id);
    }
    if(establish_job)
    {
      auto job     = establish_job;
      job->link    = serv->parent;
      job->session = nullptr;
      job->result(job);
    }
  }

  void
  session::send_keepalive(void *user)
  {
    session *self = static_cast< session * >(user);
    // if both sides agree on invalidation
    if(self->is_invalidated())
    {
      // don't send keepalive
      return;
    }
    // all zeros means keepalive
    byte_t tmp[8] = {0};
    // set flags for tx
    frame_header hdr(tmp);
    hdr.flags() = self->frame.txflags;
    // send frame after encrypting
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    self->encrypt_frame_async_send(buf.base, buf.sz);
  }

  bool
  frame_state::got_acks(frame_header hdr, size_t sz)
  {
    if(hdr.size() > sz)
    {
      llarp::Error("invalid ACKS frame size ", hdr.size(), " > ", sz);
      return false;
    }
    sz = hdr.size();
    if(sz < 12)
    {
      llarp::Error("invalid ACKS frame size ", sz, " < 12");
      return false;
    }

    auto ptr = hdr.data();
    uint64_t msgid;
    uint32_t bitmask;
    memcpy(&msgid, ptr, 8);
    memcpy(&bitmask, ptr + 8, 4);

    auto itr = tx.find(msgid);
    if(itr == tx.end())
    {
      llarp::Error("ACK for missing TX frame msgid=", msgid);
      return false;
    }

    transit_message *msg = itr->second;

    msg->ack(bitmask);

    if(msg->completed())
    {
      llarp::Debug("message transmitted msgid=", msgid);
      session *impl = static_cast< session * >(parent->impl);
      if(impl->state == session::eLIMSent && msgid == 0)
      {
        // first message acked we are established?
        impl->session_established();
      }
      tx.erase(msgid);
      delete msg;
    }
    else
    {
      llarp::Debug("message ", msgid, " retransmit fragments");
      msg->retransmit_frags(sendqueue, txflags);
    }

    return true;
  }

  llarp_link *
  session::get_parent(llarp_link_session *s)
  {
    session *link = static_cast< session * >(s->impl);
    return link->serv->parent;
  }

  void
  session::handle_verify_session_start(iwp_async_session_start *s)
  {
    session *self = static_cast< session * >(s->user);
    if(!s->buf)
    {
      // verify fail
      // TODO: remove session?
      llarp::Warn("session start verify failed from ", self->addr);
      return;
    }
    self->send_LIM();
  }

  server *
  link_alloc(struct llarp_router *router, const char *keyfile,
             struct llarp_crypto *crypto, struct llarp_logic *logic,
             struct llarp_threadpool *worker)
  {
    server *link = new server(router, crypto, logic, worker);
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

  const char *
  outboundLink_name()
  {
    return "OWP";
  }

  bool
  link_configure(struct llarp_link *l, struct llarp_ev_loop *netloop,
                 const char *ifname, int af, uint16_t port)
  {
    server *link = static_cast< server * >(l->impl);

    if(!link->ensure_privkey())
    {
      llarp::Error("failed to ensure private key");
      return false;
    }

    llarp::Debug("configure link ifname=", ifname, " af=", af, " port=", port);
    // bind
    sockaddr_in ip4addr;
    sockaddr_in6 ip6addr;
    sockaddr *addr = nullptr;
    switch(af)
    {
      case AF_INET:
        addr = (sockaddr *)&ip4addr;
        llarp::Zero(addr, sizeof(ip4addr));
        break;
      case AF_INET6:
        addr = (sockaddr *)&ip6addr;
        llarp::Zero(addr, sizeof(ip6addr));
        break;
        // TODO: AF_PACKET
      default:
        llarp::Error(__FILE__, "unsupported address family", af);
        return false;
    }

    addr->sa_family = af;

    if(!llarp::StrEq(ifname, "*"))
    {
      if(!llarp_getifaddr(ifname, af, addr))
      {
        llarp::Error("failed to get address of network interface ", ifname);
        return false;
      }
    }
    else
      l->name = outboundLink_name;

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

    link->addr         = *addr;
    link->netloop      = netloop;
    link->udp.recvfrom = &server::handle_recvfrom;
    link->udp.user     = link;
    llarp::Debug("bind IWP link to ", link->addr);
    if(llarp_ev_add_udp(link->netloop, &link->udp, link->addr) == -1)
    {
      llarp::Error("failed to bind to ", link->addr);
      return false;
    }
    return true;
  }

  bool
  link_start(struct llarp_link *l, struct llarp_logic *logic)
  {
    server *link = static_cast< server * >(l->impl);
    // give link implementations
    link->parent         = l;
    link->timeout_job_id = 0;
    link->logic          = logic;
    // start cleanup timer
    link->issue_cleanup_timer(2500);
    return true;
  }

  bool
  link_stop(struct llarp_link *l)
  {
    server *link = static_cast< server * >(l->impl);
    link->cancel_timer();
    llarp_ev_close_udp(&link->udp);
    link->clear_sessions();
    return true;
  }

  void
  link_iter_sessions(struct llarp_link *l, struct llarp_link_session_iter iter)
  {
    server *link = static_cast< server * >(l->impl);
    auto sz      = link->m_sessions.size();
    if(sz)
    {
      llarp::Debug("we have ", sz, "sessions");
      iter.link = l;
      // TODO: race condition with cleanup timer
      for(auto &item : link->m_sessions)
        if(item.second.impl)
          if(!iter.visit(&iter, &item.second))
            return;
    }
  }

  bool
  link_try_establish(struct llarp_link *l, struct llarp_link_establish_job *job)
  {
    server *link = static_cast< server * >(l->impl);
    {
      llarp::Addr dst(job->ai);
      llarp::Debug("establish session to ", dst);
      session *s = link->find_session(dst);
      if(s == nullptr)
      {
        s = link->create_session(dst);
        link->put_session(dst, s);
      }
      s->establish_job = job;
      s->frame.alive();  // mark it alive
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
    delete link;
  }

  void
  session::handle_establish_timeout(void *user, uint64_t orig, uint64_t left)
  {
    session *self          = static_cast< session * >(user);
    self->establish_job_id = 0;
    if(self->establish_job)
    {
      self->establish_job->link = self->serv->parent;
      if(left)
      {
        // timer cancelled
        self->establish_job->session = self->parent;
      }
      else
      {
        // timer timeout
        self->establish_job->session = nullptr;
      }
      self->establish_job->result(self->establish_job);
      self->establish_job = nullptr;
    }
  }

  void
  session::handle_introack_generated(iwp_async_introack *i)
  {
    session *link = static_cast< session * >(i->user);
    if(i->buf)
    {
      // track it with the server here
      if(link->serv->has_session_to(link->addr))
      {
        // duplicate session
        llarp::Warn("duplicate session to ", link->addr);
        delete link;
        return;
      }
      link->serv->put_session(link->addr, link);
      llarp::Debug("send introack to ", link->addr, " via ", link->serv->addr);
      if(llarp_ev_udp_sendto(link->udp, link->addr, i->buf, i->sz) == -1)
      {
        llarp::Warn("sendto failed");
        return;
      }
      link->EnterState(eIntroAckSent);
    }
    else
    {
      // failed to generate?
      llarp::Warn("failed to generate introack");
      delete link;
    }
  }
}

extern "C" {
void
iwp_link_init(struct llarp_link *link, struct llarp_iwp_args args)
{
  link->impl = iwp::link_alloc(args.router, args.keyfile, args.crypto,
                               args.logic, args.cryptoworker);
  link->name = iwp::link_name;
  link->get_our_address     = iwp::link_get_addr;
  link->configure           = iwp::link_configure;
  link->start_link          = iwp::link_start;
  link->stop_link           = iwp::link_stop;
  link->iter_sessions       = iwp::link_iter_sessions;
  link->try_establish       = iwp::link_try_establish;
  link->has_session_to      = iwp::server::HasSessionToRouter;
  link->sendto              = iwp::server::SendToSession;
  link->mark_session_active = iwp::link_mark_session_active;
  link->free_impl           = iwp::link_free;
}
}
