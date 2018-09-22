#include <getopt.h>
#include <signal.h>
#include <stdio.h> /* fprintf, printf */
#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <llarp.h>
#include <llarp/logic.h>
#include "dns_iptracker.hpp"
#include "dnsd.hpp"
#include "dns_dotlokilookup.hpp"
#include "ev.hpp"
#include "llarp/net.hpp"
#include "logger.hpp"

#include "router.hpp"  // for service::address

#include "crypto.hpp"  // for llarp::pubkey

#include <algorithm>  // for std::generate_n
#include <thread>     // for multithreaded version
#include <vector>

// keep this once jeff reenables concurrency
#ifdef _MSC_VER
extern "C" void
SetThreadName(DWORD dwThreadID, LPCSTR szThreadName);
#endif

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

std::string const default_chars =
    "abcdefghijklmnaoqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

#include <random>

std::string
random_string(size_t len = 15, std::string const &allowed_chars = default_chars)
{
  std::mt19937_64 gen{std::random_device()()};

  std::uniform_int_distribution< size_t > dist{0, allowed_chars.length() - 1};

  std::string ret;

  std::generate_n(std::back_inserter(ret), len,
                  [&] { return allowed_chars[dist(gen)]; });
  return ret;
}

/*
 /// check_online_request hook definition
 typedef void (*check_query_request_hook_func)(struct check_query_request *);

 struct check_query_request
 {
 bool done;
 ///hook
 check_query_request_hook_func hook;
 };

 void
 llarp_dnsd_checkQuery_resolved(struct check_query_request *request)
 {
 }
 */

struct check_query_simple_request
{
  const struct sockaddr *from;  // source
  dnsd_question_request *request;
};

std::map< std::string, struct dnsd_query_hook_response * >
    loki_tld_lookup_cache;

void
llarp_dnsd_checkQuery(void *u, uint64_t orig, uint64_t left)
{
  if(left)
    return;
  // struct check_query_request *request = static_cast< struct
  // check_query_request * >(u);
  struct check_query_simple_request *qr =
      static_cast< struct check_query_simple_request * >(u);
  dotLokiLookup *dll = (dotLokiLookup *)qr->request->user;

  // we do have result
  // if so send that
  // else
  // if we have a free private ip, send that
  struct dns_pointer *free_private = dns_iptracker_get_free(dll->ip_tracker);
  if(free_private)
  {
    // do mapping
    llarp::service::Address addr;
    if(!addr.FromString(qr->request->question.name))
    {
      llarp::LogWarn("Could not base32 decode address: ",
                     qr->request->question.name);
      delete qr;
      return;
    }
    in_addr ip_address = ((sockaddr_in *)free_private->hostResult)->sin_addr;

    bool mapResult = main_router_mapAddress(
        ctx, addr, ntohl(ip_address.s_addr));  // maybe ntohl on the s_addr
    if(!mapResult)
    {
      delete qr;
      return;
    }
    // make a dnsd_query_hook_response for the cache
    dnsd_query_hook_response *response = new dnsd_query_hook_response;
    response->dontLookUp               = true;
    response->dontSendResponse         = false;
    response->returnThis               = free_private->hostResult;
    llarp::LogInfo("Saving ", qr->request->question.name);
    loki_tld_lookup_cache[qr->request->question.name] = response;

    // FIXME: flush cache to disk
    // on crash we'll need to bring up all the same IPs we assigned before...
    writesend_dnss_response(free_private->hostResult, qr->from, qr->request);
    delete qr;
    return;
  }
  // else
  llarp::LogInfo("Sending cname to delay");
  writecname_dnss_response(
      random_string(49, "abcdefghijklmnopqrstuvwxyz") + "bob.loki", qr->from,
      qr->request);
  delete qr;
}

dnsd_query_hook_response *
hookChecker(std::string name, const struct sockaddr *from,
            struct dnsd_question_request *request)
{
  dnsd_query_hook_response *response = new dnsd_query_hook_response;
  dotLokiLookup *dll                 = (dotLokiLookup *)request->context->user;
  response->dontLookUp               = false;
  response->dontSendResponse         = false;
  response->returnThis               = nullptr;
  llarp::LogDebug("Hooked ", name);
  std::string lName = name;
  std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);

  // FIXME: probably should just read the last 5 bytes
  if(lName.find(".loki") != std::string::npos)
  {
    llarp::LogInfo("Detect Loki Lookup for ", lName);
    auto cache_check = loki_tld_lookup_cache.find(lName);
    if(cache_check != loki_tld_lookup_cache.end())
    {
      // was in cache
      llarp::LogInfo("Reused address from LokiLookupCache");
      // FIXME: avoid the allocation if you could
      delete response;
      return cache_check->second;
    }

    // decode string into HS addr
    llarp::service::Address addr;
    if(!addr.FromString(lName))
    {
      llarp::LogWarn("Could not base32 decode address");
      response->dontSendResponse = true;
      return response;
    }
    llarp::LogInfo("Got address", addr);

    // start path build early (if you're looking it up, you're probably going to
    // use it)
    main_router_prefetch(ctx, addr);

    // schedule future response
    check_query_simple_request *qr = new check_query_simple_request;
    qr->from                       = from;
    qr->request                    = request;
    // nslookup on osx is about 5 sec before a retry, 2s on linux
    llarp_logic_call_later(dll->logic, {2000, qr, &llarp_dnsd_checkQuery});

    response->dontSendResponse = true;
  }
  return response;
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
  getcwd(cwd, sizeof(cwd));
  llarp::LogInfo("Starting up server at ", cwd);

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

  if(1)
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

    struct dnsd_context dnsd;
    if(!llarp_main_init_dnsd(ctx, &dnsd, server_port,
                             (const char *)dnsr_config.upstream_host.c_str(),
                             dnsr_config.upstream_port))
    {
      llarp::LogError("Couldnt init dns daemon");
    }
    // Configure intercept
    dnsd.intercept = &hookChecker;
    dotLokiLookup dll;
    // should be a function...
    // dll.tunEndpoint = main_router_getFirstTunEndpoint(ctx);
    // dll.ip_tracker = &g_dns_iptracker;
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

    // run system and wait
    llarp_main_run(ctx);
    llarp_main_free(ctx);
  }
  else if(0)
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
    if(!llarp_dnsd_init(&dnsd, netloop, "*", server_port,
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
    if(!llarp_dnsd_init(&dnsd, nullptr, "*", server_port,
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

      raw_handle_recvfrom(&m_sockfd, (const struct sockaddr *)&clientAddress,
                          buffer, nbytes);
    }
  }

  return code;
}
