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
    Context(std::ostream &out, bool signleThread = false);
    ~Context();

    int num_nethreads   = 1;
    bool singleThreaded = false;
    std::vector< std::thread > netio_threads;
    llarp_crypto crypto;
    llarp_router *router     = nullptr;
    llarp_threadpool *worker = nullptr;
    llarp_logic *logic       = nullptr;
    llarp_config *config     = nullptr;
    llarp_nodedb *nodedb     = nullptr;
    llarp_ev_loop *mainloop  = nullptr;
    char nodedb_dir[256]     = {0};
    char conatctFile[256]    = "router.signed";

    bool
    LoadConfig(const std::string &fname);

    void
    Close();

    int
    LoadDatabase();

    int
    IterateDatabase(struct llarp_nodedb_iter i);

    bool
    PutDatabase(struct llarp_rc *rc);

    struct llarp_rc *
    GetDatabase(const byte_t *pk);

    int
    Setup();

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
}  // namespace llarp

#endif
