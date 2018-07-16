
#include <getopt.h>
#include <signal.h>
#include <stdio.h> /* fprintf, printf */
#include <unistd.h>

#include <llarp/logic.h>
#include "dnsd.hpp"
#include "ev.hpp"
#include "logger.hpp"
#include "net.hpp"

#include <thread>  // for multithreaded version
#include <vector>

bool done = false;

void
handle_signal(int sig)
{
  printf("got SIGINT\n");
  done = true;
}

int
main(int argc, char *argv[])
{
  dns_context dns;
  int code = 1;
  llarp::LogInfo("Starting up server");

  // llarp::SetLogLevel(llarp::eLogDebug);

  if(1)
  {
    // libev version
    llarp_ev_loop *netloop   = nullptr;
    llarp_threadpool *worker = nullptr;
    llarp_logic *logic       = nullptr;

    llarp_ev_loop_alloc(&netloop);

    // configure main netloop
    if(!llarp_dns_init(&dns, netloop, "127.0.0.1", 1052))
    {
      llarp::LogError("failed to initialize dns subsystem");
      return 1;
    }

    worker = llarp_init_same_process_threadpool();
    logic  = llarp_init_single_process_logic(worker);
    llarp::LogInfo("running dns mainloop");
    llarp_ev_loop_run_single_process(netloop, worker, logic);
    llarp_ev_loop_free(&netloop);
  }
  else
  {
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

      // raw_handle_recvfrom(&m_sockfd, (const struct sockaddr *)&clientAddress,
      //                    buffer, nbytes);
    }
  }

  return code;
}
