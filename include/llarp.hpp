#ifndef LLARP_HPP
#define LLARP_HPP

#include <util/types.hpp>

#include <iostream>
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
  class Logic;
  struct Router;
  struct RouterContact;

  struct Context
  {
    ~Context();

    int num_nethreads   = 1;
    bool singleThreaded = false;
    std::unique_ptr< llarp::Crypto > crypto;
    std::unique_ptr< llarp::Router > router;
    std::unique_ptr< llarp_threadpool > worker;
    std::unique_ptr< llarp::Logic > logic;
    std::unique_ptr< llarp::Config > config;
    std::unique_ptr< llarp_nodedb > nodedb;
    std::unique_ptr< llarp_ev_loop > mainloop;
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
    Setup();

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

    std::string configfile;
    std::string pidfile;
  };
}  // namespace llarp

#endif
