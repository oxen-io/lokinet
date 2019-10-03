#ifndef LLARP_H_
#define LLARP_H_
#include <sys/socket.h>
#ifdef __cplusplus
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

  /// llarp_application config
  struct llarp_config;

  /// get default config for current platform
  struct llarp_config *
  llarp_default_config();

  /// packet writer to send packets to lokinet internals
  struct llarp_vpn_writer_pipe;
  /// packet reader to recv packets from lokinet internals
  struct llarp_vpn_reader_pipe;

  /// vpn io api
  /// all hooks called in native code
  /// for use with a vpn interface managed by external code
  /// the external vpn interface MUST be up and have addresses set
  struct llarp_vpn_io
  {
    /// private implementation
    void *impl;
    /// user data
    void *user;
    /// hook set by user called by lokinet core when lokinet is done with the
    /// vpn io
    void (*closed)(struct llarp_vpn_io *);
    /// hook set by user called from lokinet core after attempting to inject
    /// into endpoint passed a bool set to true if we were injected otherwise
    /// set to false
    void (*injected)(struct llarp_vpn_io *, bool);
    /// hook set by user called every event loop tick
    void (*tick)(struct llarp_vpn_io *);
  };

  /// info about the network interface that we give to lokinet core
  struct llarp_vpn_ifaddr_info
  {
    /// name of the network interface
    char ifname[64];
    /// interface's address
    struct sockaddr_storage ifaddr;
    /// interface's netmask
    struct sockaddr_storage netmask;
  };

  /// initialize llarp_vpn_io private implementation
  void
  llarp_vpn_io_init(struct llarp_vpn_io *io);

  /// get the packet pipe for writing IP packets to lokinet internals
  /// can return nullptr
  struct llarp_vpn_pkt_writer *
  llarp_vpn_io_packet_writer(struct llarp_vpn_io *io);

  /// get the packet pipe for reading IP packets from lokinet internals
  /// can return nullptr
  struct llarp_vpn_pkt_reader *
  llarp_vpn_io_packet_reader(struct llarp_vpn_io *io);

  /// blocking read on packet reader from lokinet internals
  /// returns -1 on error, returns size of packet read
  /// thread safe
  ssize_t
  llarp_vpn_io_readpkt(struct llarp_vpn_pkt_reader *r, unsigned char *dst,
                       size_t dstlen);

  /// blocking write on packet writer to lokinet internals
  /// returns false if we can't write this packet
  /// return true if we wrote this packet
  /// thread safe
  bool
  llarp_vpn_io_writepkt(struct llarp_vpn_pkt_writer *w, unsigned char *pktbuf,
                        size_t pktlen);

  /// close vpn io and free private implementation after done
  /// operation is async and calls llarp_vpn_io.closed after fully closed
  /// after fully closed the llarp_vpn_io MUST be re-initialized by
  /// llarp_vpn_io_init if it is to be used again
  void
  llarp_vpn_io_close_async(struct llarp_vpn_io *io);

  /// get the default endpoint's name for injection
  const char *
  llarp_main_get_default_endpoint_name(struct llarp_main *m);

  /// give main context a vpn io for mobile when it is reader to do io with
  /// associated info tries to give the vpn io to endpoint with name epName a
  /// deferred call to llarp_vpn_io.injected is queued unconditionally
  /// thread safe
  void
  llarp_main_inject_vpn_by_name(struct llarp_main *m, const char *epName,
                                struct llarp_vpn_io *io,
                                struct llarp_vpn_ifaddr_info info);

  /// give main context a vpn io on its default endpoint
  static void
  llarp_main_inject_default_vpn(struct llarp_main *m, struct llarp_vpn_io *io,
                                struct llarp_vpn_ifaddr_info info)
  {
    llarp_main_inject_vpn_by_name(m, llarp_main_get_default_endpoint_name(m),
                                  io, info);
  }

  /// load config from file by name
  bool
  llarp_config_load_file(const char *fname, struct llarp_config **config);

  /// make a main context from configuration
  struct llarp_main *
  llarp_main_init_from_config(struct llarp_config *conf);

  /// initialize application context and load config
  static struct llarp_main *
  llarp_main_init(const char *fname, bool)
  {
    struct llarp_config *conf = NULL;
    if(!llarp_config_load_file(fname, &conf))
      return NULL;
    if(conf == NULL)
      return NULL;
    return llarp_main_init_from_config(conf);
  }

  /// initialize applicatin context with all defaults
  static struct llarp_main *
  llarp_main_default_init()
  {
    struct llarp_config *conf;
    conf = llarp_default_config();
    if(conf == NULL)
      return NULL;
    return llarp_main_init_from_config(conf);
  }

  /// handle signal for main context
  void
  llarp_main_signal(struct llarp_main *ptr, int sig);

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
}
#endif
#endif
