#ifndef _UPOLL_H_
#define _UPOLL_H_

#include "win32_up.h"

#if(defined(__64BIT__) || defined(__x86_64__))
#define __IS_64BIT__
#else
#define __IS_32BIT__
#endif

#if(defined WIN32 || defined _WIN32)
#undef __WINDOWS__
#define __WINDOWS__
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
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

#if defined(__WINDOWS__)
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#else
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

#if defined(__linux__)
#undef HAVE_EPOLL
#define HAVE_EPOLL 1
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#undef HAVE_POLL
#define HAVE_POLL 1
#else
#undef HAVE_SELECT
#define HAVE_SELECT 1
#endif

#if defined(HAVE_EPOLL)
#include <sys/epoll.h>
#elif defined(HAVE_POLL)
#include <poll.h>
#endif

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

#endif /* _UPOLL_H_ */
