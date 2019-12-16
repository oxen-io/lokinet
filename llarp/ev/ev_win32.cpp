#include <ev/ev_win32.hpp>

#ifdef _WIN32

#include <util/logging/logger.hpp>

// a single event queue for the TUN interface
static HANDLE tun_event_queue = INVALID_HANDLE_VALUE;

// we hand the kernel our thread handles to process completion events
static HANDLE* kThreadPool;
static int poolSize;
static CRITICAL_SECTION HandlerMtx;

// list of TUN listeners (useful for exits or other nodes with multiple TUNs)
std::list< win32_tun_io* > tun_listeners;

void
begin_tun_loop(int nThreads, llarp_ev_loop* loop)
{
  kThreadPool = new HANDLE[nThreads];
  for(int i = 0; i < nThreads; ++i)
  {
    kThreadPool[i] = CreateThread(nullptr, 0, &tun_ev_loop, loop, 0, nullptr);
  }
  llarp::LogInfo("created ", nThreads, " threads for TUN event queue");
  poolSize = nThreads;
}

// this one is called from the TUN handler
bool
win32_tun_io::queue_write(const byte_t* buf, size_t sz)
{
  do_write((void*)buf, sz);
  return true;
}

bool
win32_tun_io::setup()
{
  // Create a critical section to synchronise access to the TUN handler.
  // This *probably* has the effect of making packets move in order now
  // as only one IOCP thread will have access to the TUN handler at a
  // time
  InitializeCriticalSection(&HandlerMtx);

  if(tuntap_start(tunif, TUNTAP_MODE_TUNNEL, 0) == -1)
  {
    llarp::LogWarn("failed to start interface");
    return false;
  }
  if(tuntap_up(tunif) == -1)
  {
    char ebuf[1024];
    int err = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL, ebuf,
                  1024, nullptr);
    llarp::LogWarn("failed to put interface up: ", ebuf);
    return false;
  }

  if(tuntap_set_ip(tunif, t->ifaddr, t->ifaddr, t->netmask) == -1)
  {
    llarp::LogWarn("failed to set ip");
    return false;
  }

  if(tunif->tun_fd == INVALID_HANDLE_VALUE)
    return false;

  return true;
}

// first TUN device gets to set up the event port
bool
win32_tun_io::add_ev(llarp_ev_loop* loop)
{
  if(tun_event_queue == INVALID_HANDLE_VALUE)
  {
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    unsigned long numCPU = sys_info.dwNumberOfProcessors;
    // let the system handle 2x the number of CPUs or hardware
    // threads
    tun_event_queue = CreateIoCompletionPort(tunif->tun_fd, nullptr,
                                             (ULONG_PTR)this, numCPU * 2);
    begin_tun_loop(numCPU * 2, loop);
  }
  else
    CreateIoCompletionPort(tunif->tun_fd, tun_event_queue, (ULONG_PTR)this, 0);

  // we're already non-blocking
  // add to list
  tun_listeners.push_back(this);
  read(readbuf, 4096);
  return true;
}

// places data in event queue for kernel to process
void
win32_tun_io::do_write(void* data, size_t sz)
{
  asio_evt_pkt* pkt = new asio_evt_pkt;
  pkt->buf          = data;
  pkt->sz           = sz;
  pkt->write        = true;
  memset(&pkt->pkt, '\0', sizeof(pkt->pkt));
  WriteFile(tunif->tun_fd, data, sz, nullptr, &pkt->pkt);
}

// while this one is called from the event loop
// eventually comes back and calls queue_write()
void
win32_tun_io::flush_write()
{
  if(t->before_write)
    t->before_write(t);
}

void
win32_tun_io::read(byte_t* buf, size_t sz)
{
  asio_evt_pkt* pkt = new asio_evt_pkt;
  pkt->buf          = buf;
  memset(&pkt->pkt, '\0', sizeof(OVERLAPPED));
  pkt->sz    = sz;
  pkt->write = false;
  ReadFile(tunif->tun_fd, buf, sz, nullptr, &pkt->pkt);
}

// and now the event loop itself
extern "C" DWORD FAR PASCAL
tun_ev_loop(void* u)
{
  llarp_ev_loop* logic = static_cast< llarp_ev_loop* >(u);

  DWORD size         = 0;
  OVERLAPPED* ovl    = nullptr;
  ULONG_PTR listener = 0;
  asio_evt_pkt* pkt  = nullptr;
  BOOL alert;

  while(true)
  {
    alert = GetQueuedCompletionStatus(tun_event_queue, &size, &listener, &ovl,
                                      EV_TICK_INTERVAL);

    if(!alert)
    {
      // tick listeners on io timeout, this is required to be done every tick
      // cycle regardless of any io being done, this manages the internal state
      // of the tun logic
      for(const auto& tun : tun_listeners)
      {
        logic->call_soon([tun]() {
          if(tun->t->tick)
            tun->t->tick(tun->t);
          tun->flush_write();
        });
      }
      continue;  // let's go at it once more
    }
    if(listener == (ULONG_PTR)~0)
      break;
    // if we're here, then we got something interesting :>
    pkt              = (asio_evt_pkt*)ovl;
    win32_tun_io* ev = reinterpret_cast< win32_tun_io* >(listener);
    if(!pkt->write)
    {
      // llarp::LogInfo("read tun ", size, " bytes, pass to handler");
      // skip if our buffer remains empty
      // (if our buffer is empty, we don't even have a valid IP frame.
      // just throw it out)
      if(*(byte_t*)pkt->buf == '\0')
      {
        delete pkt;
        continue;
      }
      // EnterCriticalSection(&HandlerMtx);
      logic->call_soon([pkt, size, ev]() {
        if(ev->t->recvpkt)
          ev->t->recvpkt(ev->t, llarp_buffer_t(pkt->buf, size));
        delete pkt;
      });
      ev->read(ev->readbuf, sizeof(ev->readbuf));
      // LeaveCriticalSection(&HandlerMtx);
    }
    else
    {
      // ok let's queue another read!
      // EnterCriticalSection(&HandlerMtx);
      ev->read(ev->readbuf, sizeof(ev->readbuf));
      // LeaveCriticalSection(&HandlerMtx);
    }
    logic->call_soon([ev]() {
      if(ev->t->tick)
        ev->t->tick(ev->t);
      ev->flush_write();
    });
  }
  llarp::LogDebug("exit TUN event loop thread from system managed thread pool");
  return 0;
}

void
exit_tun_loop()
{
  if(kThreadPool)
  {
    // kill the kernel's thread pool
    // int i = (&kThreadPool)[1] - kThreadPool;  // get the size of our thread
    // pool
    llarp::LogInfo("closing ", poolSize, " threads");
    // if we get all-ones in the queue, thread exits, and we clean up
    for(int j = 0; j < poolSize; ++j)
      PostQueuedCompletionStatus(tun_event_queue, 0, ~0, nullptr);
    WaitForMultipleObjects(poolSize, kThreadPool, TRUE, INFINITE);
    for(int j = 0; j < poolSize; ++j)
      CloseHandle(kThreadPool[j]);
    delete[] kThreadPool;
    kThreadPool = nullptr;

    // the IOCP refcount is decreased each time an associated fd
    // is closed
    // the fds are closed in their destructors
    // once we get to zero, we can safely close the event port
    auto itr = tun_listeners.begin();
    while(itr != tun_listeners.end())
    {
      delete(*itr);
      itr = tun_listeners.erase(itr);
    }
    CloseHandle(tun_event_queue);
    DeleteCriticalSection(&HandlerMtx);
  }
}
/*
// now zero-copy
ssize_t
TCPWrite(llarp_tcp_conn* conn, const byte_t* ptr, size_t sz)
{
  llarp::ev_io* io = (llarp::ev_io*)conn->impl;
  return uwrite(io->fd, (char*)ptr, sz);
}

llarp_win32_loop::~llarp_win32_loop()
{
  exit_tun_loop();
}

namespace llarp
{
  int
  tcp_conn::read(byte_t* buf, size_t sz)
  {
    if(_shouldClose)
      return -1;

    ssize_t amount = uread(fd, (char*)buf, sz);

    if(amount > 0)
    {
      if(tcp.read)
        tcp.read(&tcp, llarp_buffer_t(buf, amount));
    }
    else
    {
      // error
      _shouldClose = true;
      return -1;
    }
    return 0;
  }

  void
  tcp_conn::flush_write()
  {
    connected();
    ev_io::flush_write();
  }

  ssize_t
  tcp_conn::do_write(void* buf, size_t sz)
  {
    if(_shouldClose)
      return -1;
    return uwrite(fd, (char*)buf, sz);
  }

  void
  tcp_conn::connect()
  {
    socklen_t slen = sizeof(sockaddr_in);
    if(_addr.ss_family == AF_UNIX)
      slen = sizeof(sockaddr_un);
    else if(_addr.ss_family == AF_INET6)
      slen = sizeof(sockaddr_in6);
    int result = ::connect(fd, (const sockaddr*)&_addr, slen);
    if(result == 0)
    {
      llarp::LogDebug("connected immedidately");
      connected();
    }
    // Winsock 2.x no longer returns WSAEINPROGRESS
    else if(WSAGetLastError() == WSAEWOULDBLOCK)
    {
      // in progress
      llarp::LogDebug("connect in progress");
      WSASetLastError(0);
      return;
    }
    else if(_conn->error)
    {
      // wtf?
      char ebuf[1024];
      int err = WSAGetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                    ebuf, 1024, nullptr);
      int l       = strlen(ebuf);
      ebuf[l - 2] = '\0';  // remove line break
      llarp::LogError("error connecting: ", ebuf, " [", err, "]");
      _conn->error(_conn);
    }
  }

  int
  tcp_serv::read(byte_t*, size_t)
  {
    int new_fd = ::accept(fd, nullptr, nullptr);
    if(new_fd == -1)
    {
      char ebuf[1024];
      int err = WSAGetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                    ebuf, 1024, nullptr);
      int l       = strlen(ebuf);
      ebuf[l - 2] = '\0';  // remove line break
      llarp::LogError("failed to accept on ", fd, ":", ebuf, " [", err, "]");
      return -1;
    }
    // build handler
    llarp::tcp_conn* connimpl = new tcp_conn(loop, new_fd);
    connimpl->tcp.write       = &TCPWrite;
    connimpl->tcp.loop        = loop;
    if(loop->add_ev(connimpl, true))
    {
      // call callback
      if(tcp->accepted)
        tcp->accepted(tcp, &connimpl->tcp);
      return 0;
    }
    // cleanup error
    delete connimpl;
    return -1;
  }

  bool
  udp_listener::tick()
  {
    if(udp->tick)
      udp->tick(udp);
    return true;
  }

  int
  udp_listener::read(byte_t* buf, size_t sz)
  {
    llarp_buffer_t b;
    b.base = buf;
    b.cur  = b.base;
    sockaddr_in6 src;
    socklen_t slen = sizeof(sockaddr_in6);
    sockaddr* addr = (sockaddr*)&src;
    ssize_t ret    = ::recvfrom(fd, (char*)b.base, sz, 0, addr, &slen);
    if(ret < 0)
      return -1;
    if(static_cast< size_t >(ret) > sz)
      return -1;
    b.sz = ret;
    if(udp->recvfrom)
      udp->recvfrom(udp, addr, ManagedBuffer{b});
    else
    {
      m_RecvPackets.emplace_back(
          PacketEvent{llarp::Addr(*addr), PacketBuffer(ret)});
      std::copy_n(buf, ret, m_RecvPackets.back().pkt.data());
    }
    return 0;
  }

  bool
  udp_listener::RecvMany(llarp_pkt_list* pkts)
  {
    *pkts         = std::move(m_RecvPackets);
    m_RecvPackets = llarp_pkt_list();
    return pkts->size() > 0;
  }

  static int
  UDPSendTo(llarp_udp_io* udp, const sockaddr* to, const byte_t* ptr, size_t sz)
  {
    llarp::ev_io* io = (llarp::ev_io*)udp->impl;
    return io->sendto(to, ptr, sz);
  }

  int
  udp_listener::sendto(const sockaddr* to, const void* data, size_t sz)
  {
    socklen_t slen;
    switch(to->sa_family)
    {
      case AF_INET:
        slen = sizeof(struct sockaddr_in);
        break;
      case AF_INET6:
        slen = sizeof(struct sockaddr_in6);
        break;
      default:
        return -1;
    }
    ssize_t sent = ::sendto(fd, (char*)data, sz, 0, to, slen);
    if(sent == -1)
    {
      char ebuf[1024];
      int err = WSAGetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                    ebuf, 1024, nullptr);
      llarp::LogWarn(ebuf);
    }
    return sent;
  }

};  // namespace llarp

bool
llarp_win32_loop::tcp_connect(struct llarp_tcp_connecter* tcp,
                              const sockaddr* remoteaddr)
{
  // create socket
  int fd = usocket(remoteaddr->sa_family, SOCK_STREAM, 0);
  if(fd == -1)
    return false;
  llarp::tcp_conn* conn = new llarp::tcp_conn(this, fd, remoteaddr, tcp);
  conn->tcp.write       = &TCPWrite;
  add_ev(conn, true);
  conn->connect();
  return true;
}

llarp::ev_io*
llarp_win32_loop::bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* bindaddr)
{
  int fd = usocket(bindaddr->sa_family, SOCK_STREAM, 0);
  if(fd == -1)
    return nullptr;
  socklen_t sz = sizeof(sockaddr_in);
  if(bindaddr->sa_family == AF_INET6)
  {
    sz = sizeof(sockaddr_in6);
  }
  // keep. inexplicably, windows now has unix domain sockets
  // for now, use the ID numbers directly until this comes out of
  // beta
  else if(bindaddr->sa_family == AF_UNIX)
    sz = sizeof(sockaddr_un);

  if(::bind(fd, bindaddr, sz) == -1)
  {
    uclose(fd);
    return nullptr;
  }
  if(ulisten(fd, 5) == -1)
  {
    uclose(fd);
    return nullptr;
  }
  return new llarp::tcp_serv(this, fd, tcp);
}

bool
llarp_win32_loop::udp_listen(llarp_udp_io* l, const sockaddr* src)
{
  auto ev = create_udp(l, src);
  if(ev)
    l->fd = ev->fd;
  return ev && add_ev(ev, false);
}

bool
llarp_win32_loop::running() const
{
  return (upollfd != nullptr);
}

bool
llarp_win32_loop::init()
{
  if(!upollfd)
    upollfd = upoll_create(1);
  return upollfd != nullptr;
}

// OK, the event loop, as it exists now, will _only_
// work on sockets (and not very efficiently at that).
int
llarp_win32_loop::tick(int ms)
{
  upoll_event_t events[1024];
  int result;
  result     = upoll_wait(upollfd, events, 1024, ms);
  bool didIO = false;
  if(result > 0)
  {
    int idx = 0;
    while(idx < result)
    {
      llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
      if(ev)
      {
        llarp::LogDebug(idx, " of ", result, " on ", ev->fd,
                        " events=", std::to_string(events[idx].events));
        if(events[idx].events & UPOLLERR && WSAGetLastError())
        {
          IO([&]() -> ssize_t {
            llarp::LogDebug("upoll error");
            ev->error();
            return 0;
          });
        }
        else
        {
          // write THEN READ don't revert me
          if(events[idx].events & UPOLLOUT)
          {
            IO([&]() -> ssize_t {
              llarp::LogDebug("upoll out");
              ev->flush_write();
              return 0;
            });
          }
          if(events[idx].events & UPOLLIN)
          {
            ssize_t amount = IO([&]() -> ssize_t {
              llarp::LogDebug("upoll in");
              return ev->read(readbuf, sizeof(readbuf));
            });
            if(amount > 0)
              didIO = true;
          }
        }
      }
      ++idx;
    }
  }
  if(result != -1)
    tick_listeners();
  /// if we didn't get an io events we sleep to avoid 100% cpu use
  if(!didIO)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  return result;
}

int
llarp_win32_loop::run()
{
  upoll_event_t events[1024];
  int result;
  do
  {
    result = upoll_wait(upollfd, events, 1024, EV_TICK_INTERVAL);
    if(result > 0)
    {
      int idx = 0;
      while(idx < result)
      {
        llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
        if(ev)
        {
          if(events[idx].events & UPOLLERR)
          {
            ev->error();
          }
          else
          {
            if(events[idx].events & UPOLLIN)
            {
              ev->read(readbuf, sizeof(readbuf));
            }
            if(events[idx].events & UPOLLOUT)
            {
              ev->flush_write();
            }
          }
        }
        ++idx;
      }
    }
    if(result != -1)
      tick_listeners();
  } while(upollfd);
  return result;
}

int
llarp_win32_loop::udp_bind(const sockaddr* addr)
{
  socklen_t slen;
  switch(addr->sa_family)
  {
    case AF_INET:
      slen = sizeof(struct sockaddr_in);
      break;
    case AF_INET6:
      slen = sizeof(struct sockaddr_in6);
      break;
    default:
      return -1;
  }
  int fd = usocket(addr->sa_family, SOCK_DGRAM, 0);
  if(fd == -1)
  {
    perror("usocket()");
    return -1;
  }

  if(addr->sa_family == AF_INET6)
  {
    // enable dual stack explicitly
    int dual = 1;
    if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&dual, sizeof(dual))
       == -1)
    {
      // failed
      perror("setsockopt()");
      close(fd);
      return -1;
    }
  }
  llarp::Addr a(*addr);
  llarp::LogDebug("bind to ", a);
  if(bind(fd, addr, slen) == -1)
  {
    perror("bind()");
    close(fd);
    return -1;
  }

  return fd;
}

bool
llarp_win32_loop::close_ev(llarp::ev_io* ev)
{
  return upoll_ctl(upollfd, UPOLL_CTL_DEL, ev->fd, nullptr) != -1;
}

// no tunnels here
llarp::ev_io*
llarp_win32_loop::create_tun(llarp_tun_io* tun)
{
  UNREFERENCED_PARAMETER(tun);
  return nullptr;
}

llarp::ev_io*
llarp_win32_loop::create_udp(llarp_udp_io* l, const sockaddr* src)
{
  int fd = udp_bind(src);
  if(fd == -1)
    return nullptr;
  llarp::ev_io* listener = new llarp::udp_listener(fd, l);
  l->impl                = listener;
  l->sendto              = &llarp::UDPSendTo;
  return listener;
}

bool
llarp_win32_loop::add_ev(llarp::ev_io* e, bool write)
{
  upoll_event_t ev;
  ev.data.ptr = e;
  ev.events   = UPOLLIN | UPOLLERR;
  if(write)
    ev.events |= UPOLLOUT;
  if(upoll_ctl(upollfd, UPOLL_CTL_ADD, e->fd, &ev) == -1)
  {
    delete e;
    return false;
  }
  handlers.emplace_back(e);
  return true;
}

bool
llarp_win32_loop::udp_close(llarp_udp_io* l)
{
  bool ret                      = false;
  llarp::udp_listener* listener = static_cast< llarp::udp_listener* >(l->impl);
  if(listener)
  {
    close_ev(listener);
    // remove handler
    auto itr = handlers.begin();
    while(itr != handlers.end())
    {
      if(itr->get() == listener)
        itr = handlers.erase(itr);
      else
        ++itr;
    }
    l->impl = nullptr;
    ret     = true;
  }
  return ret;
}

void
llarp_win32_loop::stop()
{
  if(upollfd)
    upoll_destroy(upollfd);
  upollfd = nullptr;
  llarp::LogDebug("destroy upoll");
}

void
llarp_win32_loop::tick_listeners()
{
  llarp_ev_loop::tick_listeners();
  for(auto& func : m_Tickers)
    LogicCall(m_Logic, func);
}
*/
#endif
