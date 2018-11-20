#ifndef LLARP_NODEDB_HPP
#define LLARP_NODEDB_HPP
#include <llarp/common.hpp>
#include <llarp/crypto.h>
#include <llarp/router_contact.hpp>
#include <llarp/router_id.hpp>

/**
 * nodedb.hpp
 *
 * persistent storage API for router contacts
 */

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

void
llarp_nodedb_set_dir(struct llarp_nodedb *n, const char *dir);

/// load entire nodedb from fs skiplist at dir
ssize_t
llarp_nodedb_load_dir(struct llarp_nodedb *n, const char *dir);

/// store entire nodedb to fs skiplist at dir
ssize_t
llarp_nodedb_store_dir(struct llarp_nodedb *n, const char *dir);

struct llarp_nodedb_iter
{
  void *user;
  llarp::RouterContact *rc;
  size_t index;
  bool (*visit)(struct llarp_nodedb_iter *);
};

/// iterate over all loaded rc with an iterator
int
llarp_nodedb_iterate_all(struct llarp_nodedb *n, struct llarp_nodedb_iter i);

/// visit all loaded rc
/// stop iteration if visit return false
void
llarp_nodedb_visit_loaded(
    struct llarp_nodedb *n,
    std::function< bool(const llarp::RouterContact &) > visit);

/// return number of RC loaded
size_t
llarp_nodedb_num_loaded(struct llarp_nodedb *n);

/**
   put an rc into the node db
   overwrites with new contents if already present
   flushes the single entry to disk
   returns true on success and false on error
 */
bool
llarp_nodedb_put_rc(struct llarp_nodedb *n, const llarp::RouterContact &rc);

bool
llarp_nodedb_get_rc(struct llarp_nodedb *n, const llarp::RouterID &pk,
                    llarp::RouterContact &result);

/**
   remove rc by public key from nodedb
   returns true if removed
 */
bool
llarp_nodedb_del_rc(struct llarp_nodedb *n, const llarp::RouterID &pk);

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
  // struct llarp_crypto *crypto; // probably don't need this because we have
  // it in the nodedb
  struct llarp_threadpool *cryptoworker;
  struct llarp_threadpool *diskworker;

  /// router contact
  llarp::RouterContact rc;
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
  llarp::PubKey pubkey;
  /// router contact result
  llarp::RouterContact result;
  /// set to true if we loaded the rc
  bool loaded;
  /// hook function called in logic thread
  llarp_async_load_rc_hook_func hook;
};

/// asynchronously load an rc from disk
void
llarp_nodedb_async_load_rc(struct llarp_async_load_rc *job);

bool
llarp_nodedb_select_random_hop(struct llarp_nodedb *n,
                               const llarp::RouterContact &prev,
                               llarp::RouterContact &result, size_t N);

#endif
