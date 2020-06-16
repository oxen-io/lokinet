#include <llarp.hpp>
#include <constants/version.hpp>

#include <config/config.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <dht/context.hpp>
#include <ev/ev.hpp>
#include <ev/vpnio.hpp>
#include <nodedb.hpp>
#include <router/router.hpp>
#include <service/context.hpp>
#include <util/logging/logger.hpp>

#include <cxxopts.hpp>
#include <csignal>
#include <stdexcept>

#if (__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

namespace llarp
{
  bool
  Context::CallSafe(std::function<void(void)> f)
  {
    return logic && LogicCall(logic, f);
  }

  bool
  Context::Configure(const RuntimeOptions& opts, std::optional<fs::path> dataDir)
  {
    if (config)
      throw std::runtime_error("Re-configure not supported");

    config = std::make_unique<Config>();

    fs::path defaultDataDir = dataDir ? *dataDir : GetDefaultDataDir();

    if (configfile.size())
    {
      if (!config->Load(configfile.c_str(), opts.isRouter, defaultDataDir))
      {
        config.release();
        llarp::LogError("failed to load config file ", configfile);
        return false;
      }
    }

    logic = std::make_shared<Logic>();

    nodedb_dir = fs::path(config->router.m_dataDir / nodedb_dirname).string();

    return true;
  }

  bool
  Context::IsUp() const
  {
    return router && router->IsRunning();
  }

  bool
  Context::LooksAlive() const
  {
    return router && router->LooksAlive();
  }

  int
  Context::LoadDatabase()
  {
    llarp_nodedb::ensure_dir(nodedb_dir.c_str());
    return 1;
  }

  void
  Context::Setup(const RuntimeOptions& opts)
  {
    llarp::LogInfo(llarp::VERSION_FULL, " ", llarp::RELEASE_MOTTO);
    llarp::LogInfo("starting up");
    if (mainloop == nullptr)
    {
      auto jobQueueSize = std::max(event_loop_queue_size, config->router.m_JobQueueSize);
      mainloop = llarp_make_ev_loop(jobQueueSize);
    }
    logic->set_event_loop(mainloop.get());

    mainloop->set_logic(logic);

    crypto = std::make_unique<sodium::CryptoLibSodium>();
    cryptoManager = std::make_unique<CryptoManager>(crypto.get());

    router = std::make_unique<Router>(mainloop, logic);

    nodedb = std::make_unique<llarp_nodedb>(
        nodedb_dir,
        [r = router.get()](std::function<void(void)> call) { r->QueueDiskIO(std::move(call)); });

    if (!router->Configure(config.get(), opts.isRouter, nodedb.get()))
      throw std::runtime_error("Failed to configure router");

    // must be done after router is made so we can use its disk io worker
    // must also be done after configure so that netid is properly set if it
    // is provided by config
    if (!this->LoadDatabase())
      throw std::runtime_error("Config::Setup() failed to load database");
  }

  int
  Context::Run(const RuntimeOptions& opts)
  {
    if (router == nullptr)
    {
      // we are not set up so we should die
      llarp::LogError("cannot run non configured context");
      return 1;
    }

    if (!opts.background)
    {
      if (!router->Run())
        return 2;
    }

    // run net io thread
    llarp::LogInfo("running mainloop");

    llarp_ev_loop_run_single_process(mainloop, logic);
    if (closeWaiter)
    {
      // inform promise if called by CloseAsync
      closeWaiter->set_value();
    }
    return 0;
  }

  void
  Context::CloseAsync()
  {
    /// already closing
    if (closeWaiter)
      return;

    if (CallSafe(std::bind(&Context::HandleSignal, this, SIGTERM)))
      closeWaiter = std::make_unique<std::promise<void>>();
  }

  void
  Context::Wait()
  {
    if (closeWaiter)
    {
      closeWaiter->get_future().wait();
      closeWaiter.reset();
    }
  }

  void
  Context::HandleSignal(int sig)
  {
    if (sig == SIGINT || sig == SIGTERM)
    {
      SigINT();
    }
    // TODO: Hot reloading would be kewl
    //       (it used to exist here, but wasn't maintained)
  }

  void
  Context::SigINT()
  {
    if (router)
    {
      /// async stop router on sigint
      router->Stop();
    }
    else
    {
      if (logic)
        logic->stop();
      llarp_ev_loop_stop(mainloop);
      Close();
    }
  }

  void
  Context::Close()
  {
    llarp::LogDebug("free config");
    config.release();

    llarp::LogDebug("free nodedb");
    nodedb.release();

    llarp::LogDebug("free router");
    router.release();

    llarp::LogDebug("free logic");
    logic.reset();
  }

#ifdef LOKINET_HIVE
  void
  Context::InjectHive(tooling::RouterHive* hive)
  {
    router->hive = hive;
  }
#endif
}  // namespace llarp

extern "C"
{
  ssize_t
  llarp_vpn_io_readpkt(struct llarp_vpn_pkt_reader* r, unsigned char* dst, size_t dstlen)
  {
    if (r == nullptr)
      return -1;
    if (not r->queue.enabled())
      return -1;
    auto pkt = r->queue.popFront();
    ManagedBuffer mbuf = pkt.ConstBuffer();
    const llarp_buffer_t& buf = mbuf;
    if (buf.sz > dstlen || buf.sz == 0)
      return -1;
    std::copy_n(buf.base, buf.sz, dst);
    return buf.sz;
  }

  bool
  llarp_vpn_io_writepkt(struct llarp_vpn_pkt_writer* w, unsigned char* pktbuf, size_t pktlen)
  {
    if (pktlen == 0 || pktbuf == nullptr)
      return false;
    if (w == nullptr)
      return false;
    llarp_vpn_pkt_queue::Packet_t pkt;
    llarp_buffer_t buf(pktbuf, pktlen);
    if (not pkt.Load(buf))
      return false;
    return w->queue.pushBack(std::move(pkt)) == llarp::thread::QueueReturn::Success;
  }

  bool
  llarp_main_inject_vpn_by_name(
      llarp::Context* ctx,
      const char* name,
      struct llarp_vpn_io* io,
      struct llarp_vpn_ifaddr_info info)
  {
    if (name == nullptr || io == nullptr)
      return false;
    if (ctx == nullptr || ctx->router == nullptr)
      return false;
    auto ep = ctx->router->hiddenServiceContext().GetEndpointByName(name);
    return ep && ep->InjectVPN(io, info);
  }

  void
  llarp_vpn_io_close_async(struct llarp_vpn_io* io)
  {
    if (io == nullptr || io->impl == nullptr)
      return;
    static_cast<llarp_vpn_io_impl*>(io->impl)->AsyncClose();
  }

  bool
  llarp_vpn_io_init(llarp::Context* ctx, struct llarp_vpn_io* io)
  {
    if (io == nullptr || ctx == nullptr)
      return false;
    llarp_vpn_io_impl* impl = new llarp_vpn_io_impl(ctx, io);
    io->impl = impl;
    return true;
  }

  struct llarp_vpn_pkt_writer*
  llarp_vpn_io_packet_writer(struct llarp_vpn_io* io)
  {
    if (io == nullptr || io->impl == nullptr)
      return nullptr;
    llarp_vpn_io_impl* vpn = static_cast<llarp_vpn_io_impl*>(io->impl);
    return &vpn->writer;
  }

  struct llarp_vpn_pkt_reader*
  llarp_vpn_io_packet_reader(struct llarp_vpn_io* io)
  {
    if (io == nullptr || io->impl == nullptr)
      return nullptr;
    llarp_vpn_io_impl* vpn = static_cast<llarp_vpn_io_impl*>(io->impl);
    return &vpn->reader;
  }
}
