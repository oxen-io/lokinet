#ifndef LLARP_HPP
#define LLARP_HPP

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
struct llarp_threadpool;

namespace llarp
{
  struct Config;
  struct Crypto;
  struct CryptoManager;
  class Logic;
  struct AbstractRouter;
  struct RouterContact;

  namespace metrics
  {
    class DefaultManagerGuard;
    class PublisherScheduler;
  }  // namespace metrics

  namespace thread
  {
    class Scheduler;
  }

  struct Context
  {
    Context();
    ~Context();

    // These come first, in this order.
    // This ensures we get metric collection on shutdown
    std::unique_ptr< thread::Scheduler > m_scheduler;
    std::unique_ptr< metrics::DefaultManagerGuard > m_metricsManager;
    std::unique_ptr< metrics::PublisherScheduler > m_metricsPublisher;

    int num_nethreads      = 1;
    bool singleThreaded    = false;
    bool disableMetrics    = false;
    bool disableMetricLogs = false;
    fs::path jsonMetricsPath;
    std::string metricTankHost;
    std::map< std::string, std::string > metricTags;

    std::unique_ptr< Crypto > crypto;
    std::unique_ptr< CryptoManager > cryptoManager;
    std::unique_ptr< AbstractRouter > router;
    std::unique_ptr< llarp_threadpool > worker;
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
    IterateDatabase(llarp_nodedb_iter &i);

    bool
    PutDatabase(struct llarp::RouterContact &rc);

    llarp::RouterContact *
    GetDatabase(const byte_t *pk);

    int
    Setup(bool debug=false);

    int
    Run();

    void
    HandleSignal(int sig);

   private:
    void
    SetPIDFile(const std::string &fname);

    bool
    WritePIDFile() const;

    void
    RemovePIDFile() const;

    bool
    Configure();

    void
    SigINT();

    bool
    ReloadConfig();

    void
    iter_config(const char *section, const char *key, const char *val);

    void
    progress();

    void
    setupMetrics();

    std::string configfile;
    std::string pidfile;
  };
}  // namespace llarp

#endif
