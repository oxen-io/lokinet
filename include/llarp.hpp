#ifndef LLARP_HPP
#define LLARP_HPP

#include <llarp.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace llarp
{
  struct Context
  {
    Context(std::ostream &stdout);
    ~Context();

    int num_nethreads = 1;
    std::vector< std::thread > netio_threads;
    llarp_alloc mem;
    llarp_crypto crypto;
    llarp_router *router     = nullptr;
    llarp_threadpool *worker = nullptr;
    llarp_logic *logic       = nullptr;
    llarp_config *config     = nullptr;
    llarp_nodedb *nodedb     = nullptr;
    llarp_ev_loop *mainloop  = nullptr;
    char nodedb_dir[256]     = {0};

    bool
    LoadConfig(const std::string &fname);

    void
    Close();

    int
    Run();

    void
    HandleSignal(int sig);

   private:
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

    std::ostream &out;
  };
}

#endif
