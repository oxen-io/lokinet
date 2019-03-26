#ifndef _UPOLL_H_
#define _UPOLL_H_

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
  int
  uread(intptr_t fd, char* buf, size_t len);
  int
  uwrite(intptr_t fd, const char* buf, size_t len);
  int
  usocketpair(intptr_t socks[2], int async);

#if(defined(__64BIT__) || defined(__x86_64__))
#define __IS_64BIT__
#else
#define __IS_32BIT__
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>

  typedef struct unote unote_t;
  typedef struct ulist ulist_t;
  typedef struct uitem uitem_t;
  typedef struct uhash uhash_t;

  struct ulist
  {
    ulist_t* next;
    ulist_t* prev;
  };

  struct uitem
  {
    ulist_t list;
    intptr_t key;
    void* val;
  };

  struct uhash
  {
    uint16_t count;
    uint16_t size;
    ulist_t* items;
  };

  struct upoll
  {
    int fd;        /* backend fd (epoll, kqueue) */
    ulist_t alive; /* all notes this queue knows about */
    uhash_t* table;
  };

  struct unote
  {
    upoll_event_t event;
    intptr_t fd;
    ulist_t queue; /* handle for the queue's notes */
    upoll_t* upoll;
  };

#define container_of(ptr, type, member) \
  ((type*)((char*)(ptr)-offsetof(type, member)))

#define ulist_init(q) \
  (q)->prev = q;      \
  (q)->next = q

#define ulist_head(h) (h)->next
#define ulist_next(q) (q)->next

#define ulist_tail(h) (h)->prev
#define ulist_prev(q) (q)->prev

#define ulist_empty(h) (h == (h)->prev)

#define ulist_append(h, x)     \
  (x)->prev       = (h)->prev; \
  (x)->prev->next = x;         \
  (x)->next       = h;         \
  (h)->prev       = x

#define ulist_insert(h, x)     \
  (x)->next       = (h)->next; \
  (x)->next->prev = x;         \
  (x)->prev       = h;         \
  (h)->next       = x

#define ulist_remove(x)        \
  (x)->next->prev = (x)->prev; \
  (x)->prev->next = (x)->next; \
  (x)->prev       = x;         \
  (x)->next       = x

#define ulist_mark(h) (h)

#define ulist_scan(q, h) \
  for((q) = ulist_head(h); (q) != ulist_mark(h); (q) = ulist_next(q))

#define ulist_data(q, type, link) container_of(q, type, link)

#ifdef __cplusplus
}
#endif

#endif /* _UPOLL_H_ */
