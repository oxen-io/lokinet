#ifndef LLARP_HPP
#define LLARP_HPP
#include <llarp.h>
#include <util/fs.hpp>
#include <util/types.hpp>
#include <ev/ev.hpp>
#include <nodedb.hpp>
#include <crypto/crypto.hpp>
#include <router/abstractrouter.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct llarp_ev_loop;

namespace llarp
{
  class Logic;
  struct Config;
  struct RouterContact;
  namespace thread
  {
    class ThreadPool;
  }

  struct RuntimeOptions
  {
    bool background = false;
    bool debug = false;
    bool isRouter = false;
  };

  struct Context
  {
    std::unique_ptr<Crypto> crypto = nullptr;
    std::unique_ptr<CryptoManager> cryptoManager = nullptr;
    std::unique_ptr<AbstractRouter> router = nullptr;
    std::shared_ptr<Logic> logic = nullptr;
    std::unique_ptr<Config> config = nullptr;
    std::unique_ptr<llarp_nodedb> nodedb = nullptr;
    llarp_ev_loop_ptr mainloop;
    std::string nodedb_dir;

    void
    Close();

    int
    LoadDatabase();

    void
    Setup(const RuntimeOptions& opts);

    int
    Run(const RuntimeOptions& opts);

    void
    HandleSignal(int sig);

    bool
    Configure(
        const RuntimeOptions& opts, std::optional<fs::path> dataDir, const fs::path& configfile);

    bool
    IsUp() const;

    bool
    LooksAlive() const;

    /// close async
    void
    CloseAsync();

    /// wait until closed and done
    void
    Wait();

    /// call a function in logic thread
    /// return true if queued for calling
    /// return false if not queued for calling
    bool
    CallSafe(std::function<void(void)> f);

    /// Creates a router. Can be overridden to allow a different class of router
    /// to be created instead. Defaults to llarp::Router.
    virtual std::unique_ptr<AbstractRouter>
    makeRouter(
        llarp_ev_loop_ptr __netloop,
        std::shared_ptr<Logic> logic);

   private:
    void
    SigINT();

    std::unique_ptr<std::promise<void>> closeWaiter;
  };

}  // namespace llarp

#endif
