#ifdef _WIN32
#pragma GCC diagnostic ignored "-Wvla"
/* emulated epoll, because the native async event system does not do
 * particularly well with notification
 */
#include "win32_upoll.h"

#define uhash_slot(K, S) (((K) ^ (K >> 8)) & (S - 1))

static uhash_t*
uhash_create(uint32_t size)
{
  unsigned int i;
  size--;
  size |= size >> 1;
  size |= size >> 2;
  size |= size >> 4;
  size |= size >> 8;
  size |= size >> 16;
  size++;

  uhash_t* hash = (uhash_t*)calloc(1, sizeof(uhash_t) + size * sizeof(ulist_t));
  hash->count   = 0;
  hash->size    = size;
  hash->items   = (ulist_t*)(((char*)hash) + sizeof(uhash_t));

  for(i = 0; i < size; i++)
  {
    ulist_init(&hash->items[i]);
  }

  return hash;
}

static void*
uhash_lookup(uhash_t* hash, intptr_t key)
{
  uint32_t slot = uhash_slot(key, hash->size);
  ulist_t* q;
  ulist_scan(q, &hash->items[slot])
  {
    uitem_t* i = ulist_data(q, uitem_t, list);
    if(i->key == key)
      return i->val;
  }
  return NULL;
}
static void
uhash_insert(uhash_t* hash, intptr_t key, void* val)
{
  uint32_t slot = uhash_slot(key, hash->size);

  uitem_t* item = (uitem_t*)calloc(1, sizeof(uitem_t));
  ulist_init(&item->list);
  item->key = key;
  item->val = val;

  ulist_append(&hash->items[slot], &item->list);
}
static int
uhash_delete(uhash_t* hash, intptr_t key)
{
  uint32_t slot = uhash_slot(key, hash->size);
  ulist_t* q;
  ulist_scan(q, &hash->items[slot])
  {
    uitem_t* i = ulist_data(q, uitem_t, list);
    if(i->key == key)
    {
      ulist_remove(q);
      free(q);
      return 1;
    }
  }
  return 0;
}
static int
uhash_destroy(uhash_t* hash)
{
  int i;
  for(i = 0; i < hash->size; i++)
  {
    while(!ulist_empty(&hash->items[i]))
    {
      ulist_t* q = ulist_next(&hash->items[i]);
      uitem_t* n = ulist_data(q, uitem_t, list);
      ulist_remove(q);
      free(n);
    }
  }
  return 0;
}

upoll_t*
upoll_create(uint32_t size)
{
  assert(size > 0);
  upoll_t* upq = (upoll_t*)calloc(1, sizeof(upoll_t));

  ulist_init(&upq->alive);

  upq->table = uhash_create(size);
  return upq;
}

void
upoll_destroy(upoll_t* upq)
{
  assert(upq != NULL);
  uhash_destroy(upq->table);
  ulist_t* q;
  unote_t* n;
  while(!ulist_empty(&upq->alive))
  {
    q = ulist_next(&upq->alive);
    n = ulist_data(n, unote_t, queue);
    ulist_remove(q);
    free(n);
  }
  free(upq);
}

int
upoll_ctl(upoll_t* upq, int op, intptr_t fd, upoll_event_t* event)
{
  if(fd < 0)
    return -EBADF;

  unote_t* note = NULL;
  switch(op)
  {
    case UPOLL_CTL_ADD:
    {
      note = (unote_t*)uhash_lookup(upq->table, fd);
      if(!note)
      {
        note        = (unote_t*)calloc(1, sizeof(unote_t));
        note->upoll = upq;
        ulist_init(&note->queue);
        note->event = *event;
        note->fd    = fd;
        ulist_append(&upq->alive, &note->queue);
        uhash_insert(upq->table, fd, (void*)note);
      }
      break;
    }
    case UPOLL_CTL_DEL:
    {
      note = (unote_t*)uhash_lookup(upq->table, fd);
      if(!note)
        return -ENOENT;
      event = &note->event;
      ulist_remove(&note->queue);
      uhash_delete(upq->table, fd);
      free(note);
      break;
    }
    case UPOLL_CTL_MOD:
    {
      note = (unote_t*)uhash_lookup(upq->table, fd);
      if(!note)
        return -ENOENT;
      note->event = *event;
      break;
    }
    default:
    {
      return -EINVAL;
    }
  }
  return 0;
}

#if defined(HAVE_POLL)
int
upoll_wait_poll(upoll_t* upq, upoll_event_t* evs, int nev, int timeout)
{
  /* FD_SETSIZE should be smaller than OPEN_MAX, but OPEN_MAX isn't portable */
  if(nev > FD_SETSIZE)
    nev = FD_SETSIZE;

  unote_t* nvec[nev];
  int r, i, nfds = 0;
  uint32_t hint;
  struct pollfd pfds[nev];

  unote_t* n = NULL;
  ulist_t* s = ulist_mark(&upq->alive);
  ulist_t* q = ulist_next(&upq->alive);

  while(q != s && nfds < nev)
  {
    n = ulist_data(q, unote_t, queue);
    q = ulist_next(q);

    ulist_remove(&n->queue);
    ulist_insert(&upq->alive, &n->queue);

    nvec[nfds]        = n;
    pfds[nfds].events = 0;
    pfds[nfds].fd     = n->fd;
    if(n->event.events & UPOLLIN)
    {
      pfds[nfds].events |= POLLIN;
    }
    if(n->event.events & UPOLLOUT)
    {
      pfds[nfds].events |= POLLOUT;
    }
    nfds++;
  }

  r = poll(pfds, nfds, timeout);
  if(r < 0)
    return -errno;

  int e = 0;
  for(i = 0; i < nfds && e < nev; i++)
  {
    hint = 0;
    if(pfds[i].revents)
    {
      n = nvec[i];
      if(pfds[i].revents & POLLIN)
        hint |= UPOLLIN;
      if(pfds[i].revents & POLLOUT)
        hint |= UPOLLOUT;
      if(pfds[i].revents & (POLLERR | POLLNVAL | POLLHUP))
        hint |= (UPOLLERR | UPOLLIN);

      if(hint & UPOLLERR)
        hint &= ~UPOLLOUT;

      evs[e].data   = n->event.data;
      evs[e].events = hint;
      ++e;
    }
  }

  return e;
}
#else
int
upoll_wait_select(upoll_t* upq, upoll_event_t* evs, int nev, int timeout)
{
  if(nev > FD_SETSIZE)
    nev = FD_SETSIZE;

  unote_t* nvec[nev];
  int i, maxfd = 0, e = 0, nfds = 0;

  fd_set pollin, pollout, pollerr;

  FD_ZERO(&pollin);
  FD_ZERO(&pollout);
  FD_ZERO(&pollerr);

  struct timeval tv;
  struct timeval* tvp = &tv;

  tv.tv_usec = 0;
  if(timeout < 0)
  {
    tvp = NULL;
  }
  else if(timeout == 0)
    tv.tv_sec = 0;
  else
  {
    tv.tv_sec  = (timeout / 1000);
    tv.tv_usec = (timeout % 1000) * 1000;
  }

  unote_t* n = NULL;
  ulist_t* s = ulist_mark(&upq->alive);
  ulist_t* q = ulist_next(&upq->alive);

  while(q != s && nfds < nev)
  {
    n = ulist_data(q, unote_t, queue);
    q = ulist_next(q);

    ulist_remove(&n->queue);
    ulist_insert(&upq->alive, &n->queue);

    nvec[nfds] = n;
    if(n->event.events & UPOLLIN)
    {
      FD_SET(n->fd, &pollin);
    }
    if(n->event.events & UPOLLOUT)
    {
      FD_SET(n->fd, &pollout);
    }
    FD_SET(n->fd, &pollerr);
    if(maxfd < n->fd)
      maxfd = n->fd;
    nfds++;
  }

#if defined(__WINDOWS__)
  int rc = select(0, &pollin, &pollout, &pollerr, tvp);
  if(rc == SOCKET_ERROR)
  {
    assert(WSAGetLastError() == WSAENOTSOCK);
    return -WSAGetLastError();
  }
#else
  int rc = select(maxfd + 1, &pollin, &pollout, &pollerr, tvp);
  if(rc == -1)
  {
    assert(errno == EINTR || errno == EBADF);
    return -errno;
  }
#endif
  e = 0;
  for(i = 0; i < nfds && e < nev; i++)
  {
    uint32_t hint = 0;
    unote_t* n    = nvec[i];
    if(FD_ISSET(n->fd, &pollin))
    {
      hint |= UPOLLIN;
    }

    if(FD_ISSET(n->fd, &pollerr))
    {
      hint |= (UPOLLERR | UPOLLIN);
    }
    else if(FD_ISSET(n->fd, &pollout))
    {
      hint |= UPOLLOUT;
    }

    if(hint)
    {
      evs[e].data   = n->event.data;
      evs[e].events = hint;
      ++e;
    }
  }
  return e;
}
#endif

int
upoll_wait(upoll_t* upq, upoll_event_t* evs, int nev, int timeout)
{
  int r = 0;
#if defined(HAVE_POLL)
  r = upoll_wait_poll(upq, evs, nev, timeout);
#else
  r = upoll_wait_select(upq, evs, nev, timeout);
#endif
  return r;
}

intptr_t
usocket(int domain, int type, int proto)
{
  intptr_t fd = (intptr_t)socket(domain, type, proto);

#if defined(__WINDOWS__)
  if(fd < 0)
    return -WSAGetLastError();
  unsigned long flags = 1;
  int rc              = ioctlsocket((SOCKET)fd, FIONBIO, &flags);
  if(rc < 0)
    return -WSAGetLastError();
#else
  if(fd < 0)
    return -errno;
  int rc = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  if(rc < 0)
    return -errno;
#endif

  return fd;
}

int
ubind(intptr_t fd, const char* host, const char* serv)
{
  struct addrinfo* info;
  struct addrinfo hint;
  memset(&hint, 0, sizeof(hint));

  int optval          = 0;
  unsigned int optlen = sizeof(optval);
#if defined(__WINDOWS__)
  int rc = getsockopt((SOCKET)fd, SOL_SOCKET, SO_TYPE, (char*)&optval,
                      (int*)&optlen);
#else
  int rc = getsockopt(fd, SOL_SOCKET, SO_TYPE, &optval, &optlen);
#endif

  hint.ai_family   = AF_INET;
  hint.ai_socktype = optval;

  rc = getaddrinfo(host, serv, &hint, &info);

  optval = 1;
  if(!rc)
  {
#if defined(__WINDOWS__)
    rc = setsockopt((SOCKET)fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval,
                    sizeof(optval));
#else
    rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
#endif
    if(!rc)
      rc = bind(fd, info->ai_addr, info->ai_addrlen);
  }

  freeaddrinfo(info);
  if(rc)
  {
#if defined(__WINDOWS__)
    return WSAGetLastError();
#else
    return errno;
#endif
  }
  return 0;
}

int
uconnect(intptr_t fd, const char* host, const char* serv)
{
  struct addrinfo* info;

  struct addrinfo hint;
  memset(&hint, 0, sizeof(hint));

  int optval = 0;
  unsigned int optlen;

#if defined(__WINDOWS__)
  int rc = getsockopt(fd, SOL_SOCKET, SO_TYPE, (char*)&optval, (int*)&optlen);
#else
  int rc = getsockopt(fd, SOL_SOCKET, SO_TYPE, &optval, &optlen);
#endif

  hint.ai_family   = AF_INET;
  hint.ai_socktype = optval;

  rc = getaddrinfo(host, serv, &hint, &info);

  if(!rc)
  {
#if defined(__WINDOWS__)
    rc = connect((SOCKET)fd, info->ai_addr, info->ai_addrlen);
#else
    rc = connect(fd, info->ai_addr, info->ai_addrlen);
#endif
  }

  freeaddrinfo(info);

  if(rc)
  {
#if defined(__WINDOWS__)
    if(WSAGetLastError() != WSAEWOULDBLOCK)
      return WSAGetLastError();
#else
    if(errno != EINPROGRESS)
      return errno;
#endif
  }
  return 0;
}

int
ulisten(intptr_t sock, int backlog)
{
  return listen(sock, backlog);
}

intptr_t
uaccept(intptr_t sock)
{
  struct sockaddr addr;

  addr.sa_family = AF_INET;
  socklen_t addr_len;

#if defined(__WINDOWS__)
  intptr_t fd = (intptr_t)accept((SOCKET)sock, &addr, &addr_len);
  if(fd == -1)
    return WSAGetLastError();
#else
  intptr_t fd = accept(sock, &addr, &addr_len);
  if(fd < 0)
    return errno;
#endif

  return fd;
}

int
uclose(intptr_t sock)
{
#if defined(__WINDOWS__)
  return closesocket((SOCKET)sock);
#else
  return close(sock);
#endif
}

int
uread(intptr_t fd, char* buf, size_t len)
{
  return recv(fd, buf, len, 0);
}
int
uwrite(intptr_t fd, const char* buf, size_t len)
{
  return send(fd, buf, len, 0);
}

/* adapted from (renamed make_overlapped to async for allergy reasons): */
/* socketpair.c
Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

    Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    The name of the author must not be used to endorse or promote products
    derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Changes:
 * 2014-02-12: merge David Woodhouse, Ger Hobbelt improvements
 *     git.infradead.org/users/dwmw2/openconnect.git/commitdiff/bdeefa54
 *     github.com/GerHobbelt/selectable-socketpair
 *   always init the socks[] to -1/INVALID_SOCKET on error, both on Win32/64
 *   and UNIX/other platforms
 * 2013-07-18: Change to BSD 3-clause license
 * 2010-03-31:
 *   set addr to 127.0.0.1 because win32 getsockname does not always set it.
 * 2010-02-25:
 *   set SO_REUSEADDR option to avoid leaking some windows resource.
 *   Windows System Error 10049, "Event ID 4226 TCP/IP has reached
 *   the security limit imposed on the number of concurrent TCP connect
 *   attempts."  Bleah.
 * 2007-04-25:
 *   preserve value of WSAGetLastError() on all error returns.
 * 2007-04-22:  (Thanks to Matthew Gregan <kinetik@flim.org>)
 *   s/EINVAL/WSAEINVAL/ fix trivial compile failure
 *   s/socket/WSASocket/ enable creation of sockets suitable as stdin/stdout
 *     of a child process.
 *   add argument make_overlapped
 */

#if defined(__WINDOWS__)
int
usocketpair(intptr_t socks[2], int async)
{
  union {
    struct sockaddr_in inaddr;
    struct sockaddr addr;
  } a;
  SOCKET listener;
  int e;
  socklen_t addrlen = sizeof(a.inaddr);
  DWORD flags       = (async ? WSA_FLAG_OVERLAPPED : 0);
  int reuse         = 1;

  if(socks == 0)
  {
    WSASetLastError(WSAEINVAL);
    return SOCKET_ERROR;
  }
  socks[0] = socks[1] = -1;

  listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if(listener == INVALID_SOCKET)
    return SOCKET_ERROR;

  memset(&a, 0, sizeof(a));
  a.inaddr.sin_family      = AF_INET;
  a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.inaddr.sin_port        = 0;

  for(;;)
  {
    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
                  (socklen_t)sizeof(reuse))
       == -1)
      break;

    if(bind(listener, &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
      break;

    memset(&a, 0, sizeof(a));
    if(getsockname(listener, &a.addr, &addrlen) == SOCKET_ERROR)
      break;
    // win32 getsockname may only set the port number, p=0.0005.
    // ( http://msdn.microsoft.com/library/ms738543.aspx ):
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_family      = AF_INET;

    if(listen(listener, 1) == SOCKET_ERROR)
      break;

    socks[0] = (intptr_t)WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags);
    if(socks[0] == -1)
      break;
    if(connect((SOCKET)socks[0], &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
      break;

    socks[1] = (intptr_t)accept(listener, NULL, NULL);
    if(socks[1] == -1)
      break;

    closesocket(listener);
    return 0;
  }

  e = WSAGetLastError();
  closesocket(listener);
  closesocket((SOCKET)socks[0]);
  closesocket((SOCKET)socks[1]);
  WSASetLastError(e);
  socks[0] = socks[1] = -1;
  return SOCKET_ERROR;
}
#else
int
usocketpair(intptr_t socks[2], int dummy)
{
  int sovec[2];
  if(socks == 0)
  {
    errno = EINVAL;
    return -1;
  }
  dummy = socketpair(AF_LOCAL, SOCK_STREAM, 0, sovec);
  if(dummy)
  {
    socks[0] = socks[1] = -1;
  }
  else
  {
    socks[0] = sovec[0];
    socks[1] = sovec[1];
  }
  return dummy;
}
#endif

#endif