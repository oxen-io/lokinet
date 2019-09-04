#ifndef LLARP_H_
#define LLARP_H_

#ifdef __cplusplus
#include <constants/version.hpp>
#include <ev/ev.h>
#include <handlers/tun.hpp>     // for handlers
#include <service/address.hpp>  // for service::address
#include <service/endpoint.hpp>
#include <util/thread/logic.hpp>
#include <util/mem.h>

extern "C"
{
#endif

  /// ensure configuration exists
  /// populate with defaults
  /// return if this succeeded
  /// if overwrite is true then overwrite old config file
  /// if basedir is not nullptr then use basedir as an absolute
  /// base path for all files in config
  bool
  llarp_ensure_config(const char *, const char *, bool overwrite,
                      bool asrouter);

  /// llarp application context for C api
  struct llarp_main;

  /// initialize application context and load config
  struct llarp_main *
  llarp_main_init(const char *fname, bool multiProcess);

  /// handle signal for main context
  void
  llarp_main_signal(struct llarp_main *ptr, int sig);

  /// give main context a vpn file descriptor pair (android/ios)
  void
  llarp_main_inject_vpn_fd(struct llarp_main *m, int rfd, int wfd);

  /// setup main context, returns 0 on success
  int
  llarp_main_setup(struct llarp_main *ptr, bool debugMode);

  /// run main context, returns 0 on success, blocks until program end
  int
  llarp_main_run(struct llarp_main *ptr);

  /// free main context and end all operations
  void
  llarp_main_free(struct llarp_main *ptr);

#ifdef __cplusplus

  void
  llarp_main_abort(struct llarp_main *ptr);

  const char *
  handleBaseCmdLineArgs(int argc, char *argv[]);

  /// load nodeDB into memory
  int
  llarp_main_loadDatabase(struct llarp_main *ptr);

  /// iterator on nodedb entries
  int
  llarp_main_iterateDatabase(struct llarp_main *ptr,
                             struct llarp_nodedb_iter i);

  /// put RC into nodeDB
  bool
  llarp_main_putDatabase(struct llarp_main *ptr,
                         struct llarp::RouterContact &rc);

  // fwd declr
  struct check_online_request;

  /// check_online_request hook definition
  typedef void (*check_online_request_hook_func)(struct check_online_request *);

  struct check_online_request
  {
    struct llarp_main *ptr;
    struct llarp_router_lookup_job *job;
    bool online;
    size_t nodes;
    bool first;
    check_online_request_hook_func hook;
  };

  /// get RC from DHT but wait until online
  void
  llarp_main_queryDHT(struct check_online_request *request);

  /// get RC from DHT
  void
  llarp_main_queryDHT_RC(struct llarp_main *ptr,
                         struct llarp_router_lookup_job *job);

  /// set up DNS libs with a context
  bool
  llarp_main_init_dnsd(struct llarp_main *ptr, struct dnsd_context *dnsd,
                       const llarp::Addr &dnsd_sockaddr,
                       const llarp::Addr &dnsc_sockaddr);

  /// set up dotLokiLookup with logic for setting timers
  bool
  llarp_main_init_dotLokiLookup(struct llarp_main *ptr,
                                struct dotLokiLookup *dll);

  llarp::RouterContact *
  llarp_main_getLocalRC(struct llarp_main *ptr);
  /// get RC from nodeDB
  llarp::RouterContact *
  llarp_main_getDatabase(struct llarp_main *ptr, byte_t *pk);

  llarp_tun_io *
  main_router_getRange(struct llarp_main *ptr);

  /// map an (host byte order) ip to a hidden service address
  bool
  main_router_mapAddress(struct llarp_main *ptr,
                         const llarp::service::Address &addr, uint32_t ip);

  /// info of possible path usage
  bool
  main_router_prefetch(struct llarp_main *ptr,
                       const llarp::service::Address &addr);
}
#endif
#endif
