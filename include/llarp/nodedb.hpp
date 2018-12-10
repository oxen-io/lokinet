#ifndef LLARP_NODEDB_HPP
#define LLARP_NODEDB_HPP
#include <llarp/common.hpp>
#include <llarp/crypto.h>
#include <llarp/router_contact.hpp>
#include <llarp/router_id.hpp>

#include <fs.hpp>

/**
 * nodedb.hpp
 *
 * persistent storage API for router contacts
 */

namespace llarp
{
  class Logic;
}

struct llarp_nodedb_iter
{
  void *user;
  llarp::RouterContact *rc;
  size_t index;
  bool (*visit)(struct llarp_nodedb_iter *);
};

struct llarp_nodedb
{
  llarp_nodedb(llarp_crypto *c) : crypto(c)
  {
  }

  ~llarp_nodedb()
  {
    Clear();
  }

  llarp_crypto *crypto;
  llarp::util::Mutex access;
  std::unordered_map< llarp::RouterID, llarp::RouterContact,
                      llarp::RouterID::Hash >
      entries;
  fs::path nodePath;

  bool
  Remove(const byte_t *pk);

  void
  Clear();

  bool
  Get(const byte_t *pk, llarp::RouterContact &result);

  bool
  Has(const byte_t *pk);

  std::string
  getRCFilePath(const byte_t *pubkey) const;

  /// insert and write to disk
  bool
  Insert(const llarp::RouterContact &rc);

  ssize_t
  Load(const fs::path &path);

  ssize_t
  loadSubdir(const fs::path &dir);

  bool
  loadfile(const fs::path &fpath);

  void
  visit(std::function< bool(const llarp::RouterContact &) > visit);

  bool
  iterate(struct llarp_nodedb_iter &i);

  void
  set_dir(const char *dir);

  ssize_t
  load_dir(const char *dir);
  ssize_t
  store_dir(const char *dir);

  int
  iterate_all(llarp_nodedb_iter i);

  size_t
  num_loaded() const;

  bool
  select_random_exit(llarp::RouterContact &rc);

  bool
  select_random_hop(const llarp::RouterContact &prev,
                    llarp::RouterContact &result, size_t N);

  static bool
  ensure_dir(const char *dir);
};

/// struct for async rc verification
struct llarp_async_verify_rc;

using llarp_async_verify_rc_hook_func =
    std::function< void(struct llarp_async_verify_rc *) >;

/// verify rc request
struct llarp_async_verify_rc
{
  /// async_verify_context
  void *user;
  /// nodedb storage
  struct llarp_nodedb *nodedb;
  // llarp::Logic for queue_job
  llarp::Logic *logic;  // includes a llarp_threadpool
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

using llarp_async_load_rc_hook_func =
    std::function< void(struct llarp_async_load_rc *) >;

struct llarp_async_load_rc
{
  /// async_verify_context
  void *user;
  /// nodedb storage
  struct llarp_nodedb *nodedb;
  /// llarp::Logic for calling hook
  llarp::Logic *logic;
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

#endif
