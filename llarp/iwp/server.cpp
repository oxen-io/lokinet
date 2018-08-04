#include <llarp/iwp/server.hpp>
#include "str.hpp"

llarp_link::llarp_link(const llarp_iwp_args& args)
    : router(args.router)
    , crypto(args.crypto)
    , logic(args.logic)
    , worker(args.cryptoworker)
    , m_name("IWP")
{
  strncpy(keyfile, args.keyfile, sizeof(keyfile));
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
llarp_link::put_intro_from(llarp_link_session* s)
{
  lock_t lock(m_PendingSessions_Mutex);
  m_PendingSessions[s->addr] = s;
}

void
llarp_link::remove_intro_from(const llarp::Addr& from)
{
  lock_t lock(m_PendingSessions_Mutex);
  m_PendingSessions.erase(from);
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
        itr->second->done();
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
        itr->second->done();
        delete itr->second;
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
        link = inner_itr->second;
      }
    }
  }
  return link && link->sendto(buf);
}

void
llarp_link::UnmapAddr(const llarp::Addr& src)
{
  lock_t lock(m_Connected_Mutex);
  // std::unordered_map< llarp::pubkey, llarp::Addr, llarp::pubkeyhash >
  auto itr = std::find_if(
      m_Connected.begin(), m_Connected.end(),
      [src](const std::pair< llarp::PubKey, llarp::Addr >& item) -> bool {
        return src == item.second;
      });
  if(itr == std::end(m_Connected))
    return;

  // tell router we are done with this session
  router->SessionClosed(itr->first);

  m_Connected.erase(itr);
}

llarp_link_session*
llarp_link::create_session(const llarp::Addr& src)
{
  return new llarp_link_session(this, seckey, src);
}

bool
llarp_link::has_session_via(const llarp::Addr& dst)
{
  lock_t lock(m_sessions_Mutex);
  return m_sessions.find(dst) != m_sessions.end();
}

llarp_link_session*
llarp_link::find_session(const llarp::Addr& addr)
{
  lock_t lock(m_sessions_Mutex);
  auto itr = m_sessions.find(addr);
  if(itr == m_sessions.end())
    return nullptr;
  else
    return itr->second;
}

void
llarp_link::put_session(const llarp::Addr& src, llarp_link_session* impl)
{
  lock_t lock(m_sessions_Mutex);
  impl->our_router = &router->rc;
  m_sessions.insert(std::make_pair(src, impl));
}

void
llarp_link::clear_sessions()
{
  lock_t lock(m_sessions_Mutex);
  auto itr = m_sessions.begin();
  while(itr != m_sessions.end())
  {
    delete itr->second;
    itr = m_sessions.erase(itr);
  }
}

void
llarp_link::iterate_sessions(std::function< bool(llarp_link_session*) > visitor)
{
  auto now = llarp_time_now_ms();
  std::list< llarp_link_session* > slist;
  {
    lock_t lock(m_sessions_Mutex);
    auto itr = m_sessions.begin();
    while(itr != m_sessions.end())
    {
      // if not timing out soon add to list to iterate on
      if(!itr->second->timedout(now, 11500))
        slist.push_back(itr->second);
      ++itr;
    }
  }
  for(auto& s : slist)
    if(!visitor(s))
      return;
}

void
llarp_link::PumpLogic()
{
  auto now = llarp_time_now_ms();
  iterate_sessions([now](llarp_link_session* s) -> bool {
    s->TickLogic(now);
    return true;
  });
}

void
llarp_link::RemoveSession(llarp_link_session* s)
{
  lock_t lock(m_sessions_Mutex);
  auto itr = m_sessions.find(s->addr);
  if(itr != m_sessions.end())
  {
    UnmapAddr(s->addr);
    s->done();
    m_sessions.erase(itr);
    delete s;
  }
}

uint8_t*
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
    if(!keygen(keyfile))
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
llarp_link::handle_recvfrom(struct llarp_udp_io* udp,
                            const struct sockaddr* saddr, const void* buf,
                            ssize_t sz)
{
  llarp_link* link = static_cast< llarp_link* >(udp->user);

  llarp_link_session* s = link->find_session(*saddr);
  if(s == nullptr)
  {
    // new inbound session
    s = link->create_session(*saddr);
  }
  s->recv(buf, sz);
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
llarp_link::get_our_address(llarp_ai* addr)
{
  addr->rank = 1;
  strncpy(addr->dialect, "IWP", sizeof(addr->dialect));
  memcpy(addr->enc_key, pubkey(), 32);
  memcpy(addr->ip.s6_addr, this->addr.addr6(), 16);
  addr->port = this->addr.port();
}

void
llarp_link::after_recv(llarp_udp_io* udp)
{
  llarp_link* self = static_cast< llarp_link* >(udp->user);
  self->PumpLogic();
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
  llarp::LogDebug("establish session to ", dst);
  llarp_link_session* s = find_session(dst);
  if(s == nullptr)
  {
    s = create_session(dst);
    put_session(dst, s);
  }
  else
    return false;
  s->establish_job = job;
  s->frame.alive();  // mark it alive
  s->introduce(job->ai.enc_key);

  return true;
}