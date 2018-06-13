#include <llarp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <experimental/filesystem>
#include <llarp/crypto.hpp>

namespace fs = std::experimental::filesystem;

static void
progress()
{
  printf(".");
  fflush(stdout);
}

struct llarp_main
{
  struct llarp_crypto crypto;
  struct llarp_router *router     = nullptr;
  struct llarp_threadpool *worker = nullptr;
  struct llarp_threadpool *thread = nullptr;
  struct llarp_logic *logic       = nullptr;
  struct llarp_config *config     = nullptr;
  struct llarp_nodedb *nodedb     = nullptr;
  struct llarp_ev_loop *mainloop  = nullptr;
  char nodedb_dir[256];
  int exitcode;

  int
  shutdown()
  {
    printf("Shutting down ");

    progress();
    if(mainloop)
      llarp_ev_loop_stop(mainloop);

    progress();
    if(worker)
      llarp_threadpool_stop(worker);

    progress();

    if(worker)
      llarp_threadpool_join(worker);

    progress();
    if(logic)
      llarp_logic_stop(logic);

    progress();

    if(router)
      llarp_stop_router(router);

    progress();
    llarp_free_router(&router);

    progress();
    llarp_free_config(&config);

    progress();
    llarp_ev_loop_free(&mainloop);

    progress();
    llarp_free_threadpool(&worker);

    progress();

    llarp_free_logic(&logic);
    progress();

    printf("\n");
    fflush(stdout);
    return exitcode;
  }
};

void
iter_main_config(struct llarp_config_iterator *itr, const char *section,
                 const char *key, const char *val)
{
  llarp_main *m = static_cast< llarp_main * >(itr->user);

  if(!strcmp(section, "router"))
  {
    if(!strcmp(key, "threads"))
    {
      int workers = atoi(val);
      if(workers > 0 && m->worker == nullptr)
      {
        m->worker = llarp_init_threadpool(workers, "llarp-worker");
      }
    }
  }
  if(!strcmp(section, "netdb"))
  {
    if(!strcmp(key, "dir"))
    {
      strncpy(m->nodedb_dir, val, sizeof(m->nodedb_dir));
    }
  }
}

llarp_main *sllarp = nullptr;

void
run_net(void *user)
{
  llarp_ev_loop_run(static_cast< llarp_ev_loop * >(user));
}

void
handle_signal(int sig)
{
  printf("\ninterrupted\n");
  llarp_ev_loop_stop(sllarp->mainloop);
  llarp_logic_stop(sllarp->logic);
}

#include <getopt.h>
#include <llarp/router_contact.h>
#include <llarp/time.h>
#include <fstream>

int
main(int argc, char *argv[])
{
  // --generate-blank /path/to/file.signed
  // --update-ifs /path/to/file.signed
  // --key /path/to/long_term_identity.key

  // --generate /path/to/file.signed
  // --update /path/to/file.signed
  // printf("has [%d]options\n", argc);
  if(argc < 3)
  {
    printf(
        "please specify --generate or --update with a path to a router contact "
        "file\n");
    return 0;
  }
  bool genMode = false;
  bool updMode = false;
  int c;
  char *rcfname;
  while(1)
  {
    static struct option long_options[] = {
        {"generate", required_argument, 0, 'g'},
        {"update", required_argument, 0, 'u'},
        {0, 0, 0, 0}};
    int option_index = 0;
    c = getopt_long(argc, argv, "gu", long_options, &option_index);
    if(c == -1)
      break;
    switch(c)
    {
      case 0:
        break;
      case 'g':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname = optarg;
        genMode = true;
        break;
      case 'u':
        // printf ("option -u with value `%s'\n", optarg);
        rcfname = optarg;
        updMode = true;
        break;
      default:
        abort();
    }
  }
  printf("parsed options\n");
  if(!genMode && !updMode)
  {
    printf("I don't know what to do, no generate or update parameter\n");
    return 1;
  }

  sllarp = new llarp_main;
  // llarp_new_config(&sllarp->config);
  // llarp_ev_loop_alloc(&sllarp->mainloop);
  llarp_crypto_libsodium_init(&sllarp->crypto);

  llarp_rc tmp;
  if(genMode)
  {
    printf("Creating [%s]\n", rcfname);
    // Jeff wanted tmp to be stack created
    // do we still need to zero it out?
    llarp_rc_clear(&tmp);
    // if we zero it out then
    // allocate fresh pointers that the bencoder can expect to be ready
    tmp.addrs = llarp_ai_list_new();
    tmp.exits = llarp_xi_list_new();
    // set updated timestamp
    tmp.last_updated = llarp_time_now_ms();
    // load longterm identity
    llarp_crypto crypt;
    llarp_crypto_libsodium_init(&crypt);
    fs::path ident_keyfile = "identity.key";
    byte_t identity[SECKEYSIZE];
    llarp_findOrCreateIdentity(&crypt, ident_keyfile.c_str(), identity);
    // get identity public key
    uint8_t *pubkey = llarp::seckey_topublic(identity);
    llarp_rc_set_pubkey(&tmp, pubkey);
    // this causes a segfault
    llarp_rc_sign(&crypt, identity, &tmp);
    // set filename
    fs::path our_rc_file = rcfname;
    // write file
    llarp_rc_write(&tmp, our_rc_file.c_str());
    // release memory for tmp lists
    llarp_rc_free(&tmp);
  }
  if(updMode)
  {
    printf("rcutil.cpp - Loading [%s]\n", rcfname);
    fs::path our_rc_file = rcfname;
    std::error_code ec;
    if(!fs::exists(our_rc_file, ec))
    {
      printf("File not found\n");
      return 0;
    }
    std::ifstream f(our_rc_file, std::ios::binary);
    if(!f.is_open())
    {
      printf("Can't open file\n");
      return 0;
    }
    byte_t tmpc[MAX_RC_SIZE];
    llarp_buffer_t buf;
    buf.base = tmpc;
    buf.cur  = buf.base;
    buf.sz   = sizeof(tmpc);
    f.read((char *)tmpc, sizeof(MAX_RC_SIZE));
    // printf("contents[%s]\n", tmpc);
    if(!llarp_rc_bdecode(&tmp, &buf))
    {
      printf("Can't decode\n");
      return 0;
    }
    // set updated timestamp
    tmp.last_updated = llarp_time_now_ms();
    // load longterm identity
    llarp_crypto crypt;
    llarp_crypto_libsodium_init(&crypt);
    fs::path ident_keyfile = "identity.key";
    byte_t identity[SECKEYSIZE];
    llarp_findOrCreateIdentity(&crypt, ident_keyfile.c_str(), identity);
    // get identity public key
    uint8_t *pubkey = llarp::seckey_topublic(identity);
    llarp_rc_set_pubkey(&tmp, pubkey);
    llarp_rc_sign(&crypt, identity, &tmp);
    // set filename
    fs::path our_rc_file_out = "update_debug.rc";
    // write file
    llarp_rc_write(&tmp, our_rc_file_out.c_str());
    // release memory for tmp lists
    llarp_rc_free(&tmp);
  }

  delete sllarp;
  return 1;
}
