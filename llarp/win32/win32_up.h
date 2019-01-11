#ifndef _UP_H_
#define _UP_H_
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#define UPOLL_CTL_ADD 1
#define UPOLL_CTL_DEL 2
#define UPOLL_CTL_MOD 3

#define UPOLLIN 0x01
#define UPOLLOUT 0x02
#define UPOLLERR 0x04
#define UPOLLET 0x08

  typedef struct upoll upoll_t;

  typedef union upoll_data {
    void* ptr;
    intptr_t fd;
    uint32_t u32;
    uint64_t u64;
  } upoll_data_t;

  typedef struct upoll_event
  {
    uint32_t events;
    upoll_data_t data;
  } upoll_event_t;

  upoll_t*
  upoll_create(uint32_t size);
  int
  upoll_ctl(upoll_t* upq, int op, intptr_t fd, upoll_event_t* event);
  int
  upoll_wait(upoll_t* upq, upoll_event_t* events, int maxevents, int timeout);
  void
  upoll_destroy(upoll_t* upq);

  intptr_t
  usocket(int domain, int type, int proto);
  intptr_t
  uaccept(intptr_t sock);

  int
  ubind(intptr_t sock, const char* name, const char* serv);
  int
  ulisten(intptr_t sock, int backlog);
  int
  uconnect(intptr_t sock, const char* name, const char* serv);
  int
  uclose(intptr_t sock);

  /* TCP sockets */
  int
  uread(intptr_t fd, char* buf, size_t len);
  int
  uwrite(intptr_t fd, const char* buf, size_t len);

  int
  usocketpair(intptr_t socks[2], int async);
#ifdef __cplusplus
}
#endif
#endif /* _UP_H_ */