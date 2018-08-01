#include <getopt.h>
#include <signal.h>
#include <stdio.h> /* fprintf, printf */
#include <unistd.h>

#include <llarp.h>
#include <llarp/logic.h>
#include "dnsd.hpp"
#include "ev.hpp"
#include "llarp/net.hpp"
#include "logger.hpp"

#include <thread>  // for multithreaded version
#include <vector>

#if(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

struct llarp_main *ctx = 0;
bool done              = false;

void
handle_signal(int sig)
{
  printf("got SIGINT\n");
  done = true;
}

sockaddr *
hookChecker(std::string name, struct dnsd_context *context)
{
  llarp::LogInfo("Hooked ", name);
  // cast your context->user;
  return nullptr;
}

// FIXME: make configurable
#define SERVER "8.8.8.8"
#define PORT 53

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
  llarp::LogInfo("Starting up server");

  const char *conffname = handleBaseCmdLineArgs(argc, argv);
  dns_relay_config dnsr_config;
  dnsr_config.upstream_host = "8.8.8.8";
  dnsr_config.upstream_port = 53;
  llarp_config *config_reader;
  llarp_new_config(&config_reader);
  // ctx      = llarp_main_init(conffname, multiThreaded);

  if(llarp_load_config(config_reader, conffname))
  {
    llarp_free_config(&config_reader);
    llarp::LogError("failed to load config file ", conffname);
    return false;
  }
  llarp_config_iterator iter;
  iter.user  = &dnsr_config;
  iter.visit = &dns_iter_config;
  llarp_config_iter(config_reader, &iter);
  llarp::LogInfo("config [", conffname, "] loaded");

  // llarp::SetLogLevel(llarp::eLogDebug);

  if(1)
  {
    // libev version
    llarp_ev_loop *netloop   = nullptr;
    llarp_threadpool *worker = nullptr;
    llarp_logic *logic       = nullptr;

    llarp_ev_loop_alloc(&netloop);

    // configure main netloop
    struct dnsd_context dnsd;
    if(!llarp_dnsd_init(&dnsd, netloop, "*", 1053,
                        (const char *)dnsr_config.upstream_host.c_str(),
                        dnsr_config.upstream_port))
    {
      // llarp::LogError("failed to initialize dns subsystem");
      llarp::LogError("Couldnt init dns daemon");
      return 0;
    }
    // Configure intercept
    dnsd.intercept = &hookChecker;

    llarp::LogInfo("singlethread start");
    worker = llarp_init_same_process_threadpool();
    logic  = llarp_init_single_process_logic(worker);
    llarp_ev_loop_run_single_process(netloop, worker, logic);
    llarp::LogInfo("singlethread end");
    llarp_ev_loop_free(&netloop);
  }
  else
  {
    
    // configure main netloop
    struct dnsd_context dnsd;
    if(!llarp_dnsd_init(&dnsd, nullptr, "*", 1053,
                        (const char *)dnsr_config.upstream_host.c_str(),
                        dnsr_config.upstream_port))
    {
      // llarp::LogError("failed to initialize dns subsystem");
      llarp::LogError("Couldnt init dns daemon");
      return 0;
    }
    // Configure intercept
    dnsd.intercept = &hookChecker;
    
    struct sockaddr_in m_address;
    int m_sockfd;

    m_sockfd                  = socket(AF_INET, SOCK_DGRAM, 0);
    m_address.sin_family      = AF_INET;
    m_address.sin_addr.s_addr = INADDR_ANY;
    m_address.sin_port        = htons(1053);
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
    if(setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
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

      raw_handle_recvfrom(&m_sockfd, (const struct sockaddr *)&clientAddress,
                          buffer, nbytes);
    }
  }

  return code;
}
