#include <llarp/iwp/server.hpp>
#include "str.hpp"

llarp_link::llarp_link(const llarp_iwp_args& args)
    : router(args.router)
    , crypto(args.crypto)
    , logic(args.logic)
    , worker(args.cryptoworker)
    , keyfile(args.keyfile)
    , m_name("IWP")
{
  iwp = llarp_async_iwp_new(crypto, logic, worker);
  pumpingLogic.store(false);
}

llarp_link::~llarp_link()
{
  llarp_async_iwp_free(iwp);
}

bool
llarp_link::has_intro_from(const llarp::Addr& from)
{
  lock_t lock(m_PendingSessions_Mutex);
  return m_PendingSessions.find(from) != m_PendingSessions.end();
}

void
llarp_link::remove_intro_from(const llarp::Addr& from)
{
  lock_t lock(m_PendingSessions_Mutex);
  m_PendingSessions.erase(from);
}

void
llarp_link::CloseSessionTo(const byte_t* pubkey)
{
  llarp::Addr addr;
  llarp::PubKey pk(pubkey);
  {
    lock_t lock(m_Connected_Mutex);
    auto itr = m_Connected.find(pk);
    if(itr == m_Connected.end())
      return;
    addr = itr->second;
  }
  {
    lock_t lock(m_sessions_Mutex);
    auto itr = m_sessions.find(addr);
    if(itr != m_sessions.end())
      itr->second->close();
  }
}

void
llarp_link::KeepAliveSessionTo(const byte_t* pubkey)
{
  llarp::Addr addr;
  llarp::PubKey pk(pubkey);
  {
    lock_t lock(m_Connected_Mutex);
    auto itr = m_Connected.find(pk);
    if(itr == m_Connected.end())
      return;
    addr = itr->second;
  }
  {
    lock_t lock(m_sessions_Mutex);
    auto itr = m_sessions.find(addr);
    if(itr != m_sessions.end())
      itr->second->keepalive();
  }
}

void
llarp_link::MapAddr(const llarp::Addr& src, const llarp::PubKey& identity)
{
  lock_t lock(m_Connected_Mutex);
  m_Connected[identity] = src;
}

bool
llarp_link::has_session_to(const byte_t* pubkey)
{
  llarp::PubKey pk(pubkey);
  lock_t lock(m_Connected_Mutex);
  return m_Connected.find(pk) != m_Connected.end();
}

void
llarp_link::TickSessions()
{
  auto now = llarp_time_now_ms();
  {
    lock_t lock(m_PendingSessions_Mutex);
    auto itr = m_PendingSessions.begin();
    while(itr != m_PendingSessions.end())
    {
      if(itr->second->timedout(now))
      {
        itr = m_PendingSessions.erase(itr);
      }
      else
        ++itr;
    }
  }
  {
    lock_t lock(m_sessions_Mutex);
    auto itr = m_sessions.begin();
    while(itr != m_sessions.end())
    {
      if(itr->second->Tick(now))
      {
        m_Connected.erase(itr->second->get_remote_router().pubkey);
        itr = m_sessions.erase(itr);
      }
      else
        ++itr;
    }
  }
}

bool
llarp_link::sendto(const byte_t* pubkey, llarp_buffer_t buf)
{
  llarp_link_session* link = nullptr;
  {
    lock_t lock(m_Connected_Mutex);
    auto itr = m_Connected.find(pubkey);
    if(itr != m_Connected.end())
    {
      lock_t innerlock(m_sessions_Mutex);
      auto inner_itr = m_sessions.find(itr->second);
      if(inner_itr != m_sessions.end())
      {
        link = inner_itr->second.get();
      }
    }
  }
  return link && link->sendto(buf);
}

llarp_link_session*
llarp_link::create_session(const llarp::Addr& src)
{
  return new llarp_link_session(this, seckey, src);
}

bool
llarp_link::has_session_via(const llarp::Addr& dst)
{
  return m_sessions.find(dst) != m_sessions.end();
}

void
llarp_link::pending_session_active(const llarp::Addr& addr)
{
  lock_t lockpending(m_PendingSessions_Mutex);
  auto itr = m_PendingSessions.find(addr);
  if(itr == m_PendingSessions.end())
    return;

  itr->second->our_router = &router->rc;
  m_sessions.insert(std::make_pair(addr, std::move(itr->second)));
  m_PendingSessions.erase(itr);
}

void
llarp_link::clear_sessions()
{
  m_sessions.clear();
}

void
llarp_link::PumpLogic()
{
  auto now = llarp_time_now_ms();
  auto itr = m_sessions.begin();
  while(itr != m_sessions.end())
  {
    itr->second->TickLogic(now);
    ++itr;
  }
}

const uint8_t*
llarp_link::pubkey()
{
  return llarp::seckey_topublic(seckey);
}

bool
llarp_link::ensure_privkey()
{
  llarp::LogDebug("ensure transport private key at ", keyfile);
  std::error_code ec;
  if(!fs::exists(keyfile, ec))
  {
    if(!keygen(keyfile.c_str()))
      return false;
  }
  std::ifstream f(keyfile);
  if(f.is_open())
  {
    f.read((char*)seckey.data(), seckey.size());
    return true;
  }
  return false;
}

bool
llarp_link::keygen(const char* fname)
{
  crypto->encryption_keygen(seckey);
  llarp::LogInfo("new transport key generated");
  std::ofstream f(fname);
  if(f.is_open())
  {
    f.write((char*)seckey.data(), seckey.size());
    return true;
  }
  return false;
}

void
llarp_link::handle_cleanup_timer(void* l, uint64_t orig, uint64_t left)
{
  if(left)
    return;
  llarp_link* link     = static_cast< llarp_link* >(l);
  link->timeout_job_id = 0;
  link->TickSessions();
  link->issue_cleanup_timer(orig);
}

void
llarp_link::visit_session(
    const llarp::Addr& fromaddr,
    std::function< void(const std::unique_ptr< llarp_link_session >&) > visit)
{
  {
    auto itr = m_sessions.find(fromaddr);
    if(itr == m_sessions.end())
    {
      auto pitr = m_PendingSessions.find(fromaddr);
      if(pitr == m_PendingSessions.end())
        visit(m_PendingSessions
                  .insert(std::make_pair(fromaddr,
                                         std::unique_ptr< llarp_link_session >(
                                             create_session(fromaddr))))
                  .first->second);
      else
        visit(pitr->second);
    }
    else
      visit(itr->second);
  }
}

void
llarp_link::handle_recvfrom(struct llarp_udp_io* udp,
                            const struct sockaddr* saddr, const void* buf,
                            ssize_t sz)
{
  llarp_link* link = static_cast< llarp_link* >(udp->user);

  link->visit_session(
      *saddr, [buf, sz](const std::unique_ptr< llarp_link_session >& s) {
        s->recv(buf, sz);
      });
}

void
llarp_link::cancel_timer()
{
  if(timeout_job_id)
  {
    llarp_logic_cancel_call(logic, timeout_job_id);
  }
  timeout_job_id = 0;
}

void
llarp_link::issue_cleanup_timer(uint64_t timeout)
{
  timeout_job_id = llarp_logic_call_later(
      logic, {timeout, this, &llarp_link::handle_cleanup_timer});
}

void
llarp_link::get_our_address(llarp::AddressInfo& addr)
{
  addr.rank    = 1;
  addr.dialect = "IWP";
  addr.pubkey  = pubkey();
  addr.port    = this->addr.port();
  memcpy(addr.ip.s6_addr, this->addr.addr6(), 16);
}

void
llarp_link::after_recv(llarp_udp_io* udp)
{
  llarp_link* self = static_cast< llarp_link* >(udp->user);
  llarp_logic_queue_job(
      self->logic,
      {self, [](void* u) { static_cast< llarp_link* >(u)->PumpLogic(); }});
}

bool
llarp_link::configure(struct llarp_ev_loop* netloop, const char* ifname, int af,
                      uint16_t port)
{
  if(!ensure_privkey())
  {
    llarp::LogError("failed to ensure private key");
    return false;
  }

  llarp::LogDebug("configure link ifname=", ifname, " af=", af, " port=", port);
  // bind
  sockaddr_in ip4addr;
  sockaddr_in6 ip6addr;
  sockaddr* addr = nullptr;
  switch(af)
  {
    case AF_INET:
      addr = (sockaddr*)&ip4addr;
      llarp::Zero(addr, sizeof(ip4addr));
      break;
    case AF_INET6:
      addr = (sockaddr*)&ip6addr;
      llarp::Zero(addr, sizeof(ip6addr));
      break;
    // TODO: AF_PACKET
    default:
      llarp::LogError(__FILE__, "unsupported address family", af);
      return false;
  }

  addr->sa_family = af;

  if(!llarp::StrEq(ifname, "*"))
  {
    if(!llarp_getifaddr(ifname, af, addr))
    {
      llarp::LogError("failed to get address of network interface ", ifname);
      return false;
    }
  }
  else
    m_name = "OWP";  // outboundLink_name;

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

  this->addr    = *addr;
  this->netloop = netloop;
  udp.recvfrom  = &llarp_link::handle_recvfrom;
  udp.user      = this;
  udp.tick      = &llarp_link::after_recv;
  llarp::LogDebug("bind IWP link to ", addr);
  if(llarp_ev_add_udp(netloop, &udp, addr) == -1)
  {
    llarp::LogError("failed to bind to ", addr);
    return false;
  }
  return true;
}

bool
llarp_link::start_link(struct llarp_logic* pLogic)
{
  // give link implementations
  // link->parent         = l;
  timeout_job_id = 0;
  this->logic    = pLogic;
  // start cleanup timer
  issue_cleanup_timer(500);
  return true;
}

bool
llarp_link::stop_link()
{
  cancel_timer();
  llarp_ev_close_udp(&udp);
  clear_sessions();
  return true;
}

bool
llarp_link::try_establish(struct llarp_link_establish_job* job)
{
  llarp::Addr dst(job->ai);
  if(has_session_via(dst))
    return false;
  if(m_PendingSessions.find(dst) == m_PendingSessions.end())
  {
    llarp::LogDebug("establish session to ", dst);
    visit_session(dst, [job](const std::unique_ptr< llarp_link_session >& s) {
      s->establish_job = job;
      s->frame.alive();  // mark it alive
      s->introduce(job->ai.pubkey);
    });
    return true;
  }
  else
    return false;
}
