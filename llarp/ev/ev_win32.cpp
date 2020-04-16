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
std::list<win32_tun_io*> tun_listeners;

void
begin_tun_loop(int nThreads, llarp_ev_loop* loop)
{
  kThreadPool = new HANDLE[nThreads];
  for (int i = 0; i < nThreads; ++i)
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

  if (tuntap_start(tunif, TUNTAP_MODE_TUNNEL, 0) == -1)
  {
    llarp::LogWarn("failed to start interface");
    return false;
  }
  if (tuntap_up(tunif) == -1)
  {
    char ebuf[1024];
    int err = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL, ebuf, 1024, nullptr);
    llarp::LogWarn("failed to put interface up: ", ebuf);
    return false;
  }
  tunif->bindaddr = t->dnsaddr;

  if (tuntap_set_ip(tunif, t->ifaddr, t->ifaddr, t->netmask) == -1)
  {
    llarp::LogWarn("failed to set ip");
    return false;
  }

  if (tunif->tun_fd == INVALID_HANDLE_VALUE)
    return false;

  return true;
}

// first TUN device gets to set up the event port
bool
win32_tun_io::add_ev(llarp_ev_loop* loop)
{
  if (tun_event_queue == INVALID_HANDLE_VALUE)
  {
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    unsigned long numCPU = sys_info.dwNumberOfProcessors;
    // let the system handle 2x the number of CPUs or hardware
    // threads
    tun_event_queue = CreateIoCompletionPort(tunif->tun_fd, nullptr, (ULONG_PTR)this, numCPU * 2);
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
  pkt->buf = data;
  pkt->sz = sz;
  pkt->write = true;
  memset(&pkt->pkt, '\0', sizeof(pkt->pkt));
  WriteFile(tunif->tun_fd, data, sz, nullptr, &pkt->pkt);
}

// while this one is called from the event loop
// eventually comes back and calls queue_write()
void
win32_tun_io::flush_write()
{
  if (t->before_write)
    t->before_write(t);
}

void
win32_tun_io::read(byte_t* buf, size_t sz)
{
  asio_evt_pkt* pkt = new asio_evt_pkt;
  pkt->buf = buf;
  memset(&pkt->pkt, '\0', sizeof(OVERLAPPED));
  pkt->sz = sz;
  pkt->write = false;
  ReadFile(tunif->tun_fd, buf, sz, nullptr, &pkt->pkt);
}

// and now the event loop itself
extern "C" DWORD FAR PASCAL
tun_ev_loop(void* u)
{
  llarp_ev_loop* logic = static_cast<llarp_ev_loop*>(u);

  DWORD size = 0;
  OVERLAPPED* ovl = nullptr;
  ULONG_PTR listener = 0;
  asio_evt_pkt* pkt = nullptr;
  BOOL alert;

  while (true)
  {
    alert = GetQueuedCompletionStatus(tun_event_queue, &size, &listener, &ovl, EV_TICK_INTERVAL);

    if (!alert)
    {
      // tick listeners on io timeout, this is required to be done every tick
      // cycle regardless of any io being done, this manages the internal state
      // of the tun logic
      for (const auto& tun : tun_listeners)
      {
        logic->call_soon([tun]() {
          tun->flush_write();
          if (tun->t->tick)
            tun->t->tick(tun->t);
        });
      }
      continue;  // let's go at it once more
    }
    if (listener == (ULONG_PTR)~0)
      break;
    // if we're here, then we got something interesting :>
    pkt = (asio_evt_pkt*)ovl;
    win32_tun_io* ev = reinterpret_cast<win32_tun_io*>(listener);
    if (!pkt->write)
    {
      // llarp::LogInfo("read tun ", size, " bytes, pass to handler");
      logic->call_soon([pkt, size, ev]() {
        if (ev->t->recvpkt)
          ev->t->recvpkt(ev->t, llarp_buffer_t(pkt->buf, size));
        delete pkt;
      });
      ev->read(ev->readbuf, sizeof(ev->readbuf));
    }
    else
    {
      // ok let's queue another read!
      ev->read(ev->readbuf, sizeof(ev->readbuf));
    }
    logic->call_soon([ev]() {
      ev->flush_write();      
      if(ev->t->tick)
        ev->t->tick(ev->t);
    });
  }
  llarp::LogDebug("exit TUN event loop thread from system managed thread pool");
  return 0;
}

void
exit_tun_loop()
{
  if (kThreadPool)
  {
    // kill the kernel's thread pool
    // int i = (&kThreadPool)[1] - kThreadPool;  // get the size of our thread
    // pool
    llarp::LogInfo("closing ", poolSize, " threads");
    // if we get all-ones in the queue, thread exits, and we clean up
    for (int j = 0; j < poolSize; ++j)
      PostQueuedCompletionStatus(tun_event_queue, 0, ~0, nullptr);
    WaitForMultipleObjects(poolSize, kThreadPool, TRUE, INFINITE);
    for (int j = 0; j < poolSize; ++j)
      CloseHandle(kThreadPool[j]);
    delete[] kThreadPool;
    kThreadPool = nullptr;

    // the IOCP refcount is decreased each time an associated fd
    // is closed
    // the fds are closed in their destructors
    // once we get to zero, we can safely close the event port
    auto itr = tun_listeners.begin();
    while (itr != tun_listeners.end())
    {
      delete (*itr);
      itr = tun_listeners.erase(itr);
    }
    CloseHandle(tun_event_queue);
    DeleteCriticalSection(&HandlerMtx);
  }
}

#endif
