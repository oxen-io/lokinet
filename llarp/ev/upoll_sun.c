#include "upoll_sun.h"

#define uhash_slot(K, S) (((K) ^ (K >> 8)) & (S - 1))

static uhash_t*
uhash_create(uint32_t size)
{
  int i;
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

int
upoll_wait(upoll_t* upq, upoll_event_t* evs, int nev, int timeout)
{
  int r = 0;
  r     = upoll_wait_poll(upq, evs, nev, timeout);
  return r;
}