#ifndef LLARP_HPP
#define LLARP_HPP

#include <util/types.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct llarp_config;
struct llarp_config_iterator;
struct llarp_ev_loop;
struct llarp_nodedb;
struct llarp_nodedb_iter;
struct llarp_threadpool;

namespace llarp
{
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
    llarp::Router *router    = nullptr;
    llarp_threadpool *worker = nullptr;
    llarp::Logic *logic      = nullptr;
    llarp_config *config     = nullptr;
    llarp_nodedb *nodedb     = nullptr;
    llarp_ev_loop *mainloop  = nullptr;
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

    static void
    iter_config(llarp_config_iterator *itr, const char *section,
                const char *key, const char *val);

    void
    progress();

    std::string configfile;
    std::string pidfile;
  };
}  // namespace llarp

#endif
