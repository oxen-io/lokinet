#include <unistd.h>
#include <llarp.h>
#include <llarp/dns_iptracker.hpp>
#include <llarp/dnsd.hpp>
#include <llarp/dns_dotlokilookup.hpp>

#include <llarp/threading.hpp>  // for multithreaded version (multiplatorm)

#include <signal.h>  // Linux needs this for SIGINT

#ifdef _WIN32
#define uint UINT
#endif

#if(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

// CHECK: is multiprocess still a thing?
#ifndef TESTNET
#define TESTNET 0
#endif

struct llarp_main *ctx = 0;
bool done              = false;

void
handle_signal(int sig)
{
  printf("got SIGINT\n");
  done = true;
  // if using router, signal it
  if(ctx)
    llarp_main_signal(ctx, sig);
}

struct dns_relay_config
{
  std::string upstream_host;
  uint16_t upstream_port;
};

void
dns_iter_config(llarp_config_iterator *itr, const char *section,
                const char *key, const char *val)
{
  dns_relay_config *config = (dns_relay_config *)itr->user;
  if(!strcmp(section, "dns"))
  {
    if(!strcmp(key, "upstream-server"))
    {
      config->upstream_host = strdup(val);
      llarp::LogDebug("Config file setting dns server to ",
                      config->upstream_host);
    }
    if(!strcmp(key, "upstream-port"))
    {
      config->upstream_port = atoi(val);
      llarp::LogDebug("Config file setting dns server port to ",
                      config->upstream_port);
    }
  }
}

int
main(int argc, char *argv[])
{
  int code = 1;
  char cwd[1024];
  char *ptr = getcwd(cwd, sizeof(cwd));
  llarp::LogInfo("Starting up server at ", ptr);

  const char *conffname = handleBaseCmdLineArgs(argc, argv);
  dns_relay_config dnsr_config;
  dnsr_config.upstream_host = "8.8.8.8";
  dnsr_config.upstream_port = 53;
  llarp_config *config_reader;
  llarp_new_config(&config_reader);

  if(llarp_load_config(config_reader, conffname))
  {
    llarp_free_config(&config_reader);
    llarp::LogError("failed to load config file ", conffname);
    return 0;
  }
  llarp_config_iterator iter;
  iter.user  = &dnsr_config;
  iter.visit = &dns_iter_config;
  llarp_config_iter(config_reader, &iter);
  llarp::LogInfo("config [", conffname, "] loaded");

  const uint16_t server_port = 53;

  dns_iptracker_init();

  // llarp::SetLogLevel(llarp::eLogDebug);
  
  bool enableDLL = false;
  bool useLlarp = true;

  if(enableDLL)
  {
    // libev version w/router context
    ctx = llarp_main_init(conffname, !TESTNET);
    if(!ctx)
    {
      llarp::LogError("Cant set up context");
      return 0;
    }
    llarp_main_setup(ctx);
    signal(SIGINT, handle_signal);

    // we can't programmatic force a client
    // but we'll need to be one...

    /*
    struct dnsd_context dnsd;
    llarp::Addr dnsd_sockaddr(127, 0, 0, 1, 53);
    llarp::Addr dnsc_sockaddr(dnsr_config.upstream_host,
                              dnsr_config.upstream_port);
    // server_port, (const char *)dnsr_config.upstream_host.c_str(),
    // dnsr_config.upstream_port
    if(!llarp_main_init_dnsd(ctx, &dnsd, dnsd_sockaddr, dnsc_sockaddr))
    {
      llarp::LogError("Couldnt init dns daemon");
    }
    // Configure intercept
    dnsd.intercept = &llarp_dotlokilookup_handler;
    dotLokiLookup dll;
     */
    // should be a function...
    // dll.tunEndpoint = main_router_getFirstTunEndpoint(ctx);
    // dll.ip_tracker = &g_dns_iptracker;
    /*
    llarp_main_init_dotLokiLookup(ctx, &dll);
    dnsd.user = &dll;

    // check tun set up
    llarp_tun_io *tun = main_router_getRange(ctx);
    llarp::LogDebug("TunNetmask: ", tun->netmask);
    llarp::LogDebug("TunIfAddr: ", tun->ifaddr);

    // configure dns_ip_tracker to use this
    // well our routes table should already be set up

    // mark our TunIfAddr as used
    if(tun)
    {
      dll.user = tun;
      struct sockaddr_in addr;
      addr.sin_addr.s_addr = inet_addr(tun->ifaddr);
      addr.sin_family      = AF_INET;

      llarp::Addr tunIp(addr);
      llarp::LogDebug("llarp::TunIfAddr: ", tunIp);
      dns_iptracker_setup_dotLokiLookup(&dll, tunIp);
      dns_iptracker_setup(tunIp);
    }
    else
    {
      llarp::LogWarn("No tun interface, can't look up .loki");
    }
    */

    // run system and wait
    llarp_main_run(ctx);
    llarp_main_free(ctx);
  }
  else if(useLlarp)
  {
    // libev version
    llarp_ev_loop *netloop   = nullptr;
    llarp_threadpool *worker = nullptr;
    llarp_logic *logic       = nullptr;

    llarp_ev_loop_alloc(&netloop);  // set up netio worker
    worker = llarp_init_same_process_threadpool();
    logic  = llarp_init_single_process_logic(worker);  // set up logic worker

    // configure main netloop
    struct dnsd_context dnsd;
    llarp::Addr dnsd_sockaddr(127, 0, 0, 1, 53);
    llarp::Addr dnsc_sockaddr(dnsr_config.upstream_host,
                              dnsr_config.upstream_port);
    llarp::LogInfo("dnsd_sockaddr init: ", dnsd_sockaddr);
    llarp::LogInfo("dnsc_sockaddr init: ", dnsc_sockaddr);
    if(!llarp_dnsd_init(&dnsd, logic, netloop, dnsd_sockaddr, dnsc_sockaddr))
    {
      // llarp::LogError("failed to initialize dns subsystem");
      llarp::LogError("Couldnt init dns daemon");
      return 0;
    }
    // Configure intercept
    dnsd.intercept = &llarp_dotlokilookup_handler;

    llarp::LogInfo("singlethread start");
    llarp_ev_loop_run_single_process(netloop, worker, logic);
    llarp::LogInfo("singlethread end");

    llarp_ev_loop_free(&netloop);
  }
  else
  {
    // need this for timer stuff
    llarp_threadpool *worker = nullptr;
    llarp_logic *logic       = nullptr;
    worker                   = llarp_init_same_process_threadpool();
    logic = llarp_init_single_process_logic(worker);  // set up logic worker

    // configure main netloop
    struct dnsd_context dnsd;
    llarp::Addr dnsd_sockaddr(127, 0, 0, 1, 53);
    llarp::Addr dnsc_sockaddr(dnsr_config.upstream_host,
                              dnsr_config.upstream_port);
    if(!llarp_dnsd_init(&dnsd, logic, nullptr, dnsd_sockaddr, dnsc_sockaddr))
    {
      // llarp::LogError("failed to initialize dns subsystem");
      llarp::LogError("Couldnt init dns daemon");
      return 0;
    }
    // Configure intercept
    dnsd.intercept = &llarp_dotlokilookup_handler;

    struct sockaddr_in m_address;
    int m_sockfd;

#ifndef _WIN32
    m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
#else
    m_sockfd =
        WSASocket(AF_INET, SOCK_DGRAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
#endif
    m_address.sin_family      = AF_INET;
    m_address.sin_addr.s_addr = INADDR_ANY;
    m_address.sin_port        = htons(server_port);
    int rbind                 = bind(m_sockfd, (struct sockaddr *)&m_address,
                     sizeof(struct sockaddr_in));

    if(rbind != 0)
    {
      llarp::LogError("Could not bind: ", strerror(errno));
      return 0;
    }

    const size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];  // 1024 is buffer size
    struct sockaddr_in clientAddress;
    socklen_t addrLen = sizeof(struct sockaddr_in);

    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 100 * 1000;  // 1 sec
#ifndef _WIN32
    if(setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
#else
    if(setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
                  sizeof(tv))
       < 0)
#endif
    {
      perror("Error");
    }

    signal(SIGINT, handle_signal);
    while(!done)
    {
      // sigint quits after next packet
      int nbytes = recvfrom(m_sockfd, buffer, BUFFER_SIZE, 0,
                            (struct sockaddr *)&clientAddress, &addrLen);
      if(nbytes == -1)
        continue;
      llarp::LogInfo("Received Bytes ", nbytes);
      
      llarp_buffer_t lbuffer;
      lbuffer.base = (byte_t *)buffer;
      lbuffer.cur  = lbuffer.base;
      lbuffer.sz   = nbytes;
      
      dns_msg_header *hdr = decode_hdr(lbuffer);

      // if we sent this out, then there's an id
      struct dns_tracker *tracker = (struct dns_tracker *)dnsd.client.tracker;
      struct dnsc_answer_request *request = tracker->client_request[hdr->id].get();
      
      if (request)
      {
        request->packet.header = hdr;
        generic_handle_dnsc_recvfrom(tracker->client_request[hdr->id].get(), lbuffer, hdr);
      }
      else
      {
        llarp::LogWarn("Ignoring multiple responses on ID #", hdr->id);
      }
      
      //raw_handle_recvfrom(&m_sockfd, (const struct sockaddr *)&clientAddress,
      //                    buffer, nbytes);
    }
  }

  return code;
}
