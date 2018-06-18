#ifndef LLARP_NODEDB_H
#define LLARP_NODEDB_H
#include <llarp/common.h>
#include <llarp/crypto.h>
#include <llarp/router_contact.h>

/**
 * nodedb.h
 *
 * persistent storage API for router contacts
 */

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_nodedb;

/// create an empty nodedb
struct llarp_nodedb *
llarp_nodedb_new(struct llarp_crypto *crypto);

/// free a nodedb and all loaded rc
void
llarp_nodedb_free(struct llarp_nodedb **n);

/// ensure a nodedb fs skiplist structure is at dir
/// create if not there.
bool
llarp_nodedb_ensure_dir(const char *dir);

/// load entire nodedb from fs skiplist at dir
ssize_t
llarp_nodedb_load_dir(struct llarp_nodedb *n, const char *dir);

/// store entire nodedb to fs skiplist at dir
ssize_t
llarp_nodedb_store_dir(struct llarp_nodedb *n, const char *dir);

struct llarp_nodedb_iter
{
  void *user;
  struct llarp_rc *rc;
  bool (*visit)(struct llarp_nodedb_iter *);
};

/// iterate over all loaded rc with an iterator
void
llarp_nodedb_iterate_all(struct llarp_nodedb *n, struct llarp_nodedb_iter i);

/**
   put an rc into the node db
   overwrites with new contents if already present
   flushes the single entry to disk
   returns true on success and false on error
 */
bool
llarp_nodedb_put_rc(struct llarp_nodedb *n, struct llarp_rc *rc);

/// return a pointer to an already loaded RC or nullptr if it's not there
struct llarp_rc *
llarp_nodedb_get_rc(struct llarp_nodedb *n, const byte_t *pk);

/// struct for async rc verification
struct llarp_async_verify_rc;

typedef void (*llarp_async_verify_rc_hook_func)(struct llarp_async_verify_rc *);

/// verify rc request
struct llarp_async_verify_rc
{
  /// async_verify_context
  void *user;
  /// nodedb storage
  struct llarp_nodedb *nodedb;
  // llarp_logic for llarp_logic_queue_job
  struct llarp_logic *logic;  // includes a llarp_threadpool
  // struct llarp_crypto *crypto; // probably don't need this because we have it
  // in the nodedb
  struct llarp_threadpool *cryptoworker;
  struct llarp_threadpool *diskworker;

  /// router contact (should this be a pointer?)
  struct llarp_rc rc;
  /// result
  bool valid;
  /// hook
  llarp_async_verify_rc_hook_func hook;
};

/**
    struct for async rc verification
    data is loaded in disk io threadpool
    crypto is done on the crypto worker threadpool
    result is called on the logic thread
*/
void
llarp_nodedb_async_verify(struct llarp_async_verify_rc *job);

struct llarp_async_load_rc;

typedef void (*llarp_async_load_rc_hook_func)(struct llarp_async_load_rc *);

struct llarp_async_load_rc
{
  /// async_verify_context
  void *user;
  /// nodedb storage
  struct llarp_nodedb *nodedb;
  /// llarp_logic for calling hook
  struct llarp_logic *logic;
  /// disk worker threadpool
  struct llarp_threadpool *diskworker;
  /// target pubkey
  byte_t pubkey[PUBKEYSIZE];
  /// router contact result
  struct llarp_rc rc;
  /// set to true if we loaded the rc
  bool loaded;
  /// hook function called in logic thread
  llarp_async_load_rc_hook_func hook;
};

/// asynchronously load an rc from disk
void
llarp_nodedb_async_load_rc(struct llarp_async_load_rc *job);

#ifdef __cplusplus
}
#endif
#endif
