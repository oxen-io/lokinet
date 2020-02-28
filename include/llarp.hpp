#ifndef LLARP_HPP
#define LLARP_HPP
#include <llarp.h>
#include <util/fs.hpp>
#include <util/types.hpp>
#include <ev/ev.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct llarp_ev_loop;
struct llarp_nodedb;
struct llarp_nodedb_iter;
struct llarp_main;

#ifdef LOKINET_HIVE
namespace tooling
{
  struct RouterHive;
} // namespace tooling
#endif

namespace llarp
{
  class Logic;
  struct AbstractRouter;
  struct Config;
  struct Crypto;
  struct CryptoManager;
  struct RouterContact;
  namespace thread
  {
    class ThreadPool;
  }

  struct Context
  {
    /// get context from main pointer
    static std::shared_ptr<Context> 
    Get(llarp_main *);

    Context() = default;

    std::unique_ptr< Crypto > crypto;
    std::unique_ptr< CryptoManager > cryptoManager;
    std::unique_ptr< AbstractRouter > router;
    std::shared_ptr< thread::ThreadPool > worker;
    std::shared_ptr< Logic > logic;
    std::unique_ptr< Config > config;
    std::unique_ptr< llarp_nodedb > nodedb;
    llarp_ev_loop_ptr mainloop;
    std::string nodedb_dir;

    bool
    LoadConfig(const std::string &fname);

    void
    Close();

    int
    LoadDatabase();

    int
    Setup();

    int
    Run(llarp_main_runtime_opts opts);

    void
    HandleSignal(int sig);

    bool
    Configure();

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
    CallSafe(std::function< void(void) > f);

#ifdef LOKINET_HIVE
    void
    InjectHive(tooling::RouterHive* hive);
#endif

   private:
    void
    SetPIDFile(const std::string &fname);

    bool
    WritePIDFile() const;

    void
    RemovePIDFile() const;

    void
    SigINT();

    bool
    ReloadConfig();

    std::string configfile;
    std::string pidfile;
    std::unique_ptr< std::promise< void > > closeWaiter;
  };
}  // namespace llarp

#endif
