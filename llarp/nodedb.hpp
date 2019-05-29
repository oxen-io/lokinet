#ifndef LLARP_NODEDB_HPP
#define LLARP_NODEDB_HPP

#include <router_contact.hpp>
#include <router_id.hpp>
#include <util/common.hpp>
#include <util/fs.hpp>
#include <util/threading.hpp>

#include <absl/base/thread_annotations.h>

#include <set>

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

/**
 * nodedb.hpp
 *
 * persistent storage API for router contacts
 */

struct llarp_threadpool;

namespace llarp
{
  class Logic;

  namespace thread
  {
    class ThreadPool;
  }
}  // namespace llarp

struct llarp_nodedb_iter
{
  void *user;
  llarp::RouterContact *rc;
  size_t index;
  bool (*visit)(struct llarp_nodedb_iter *);
};

struct llarp_nodedb
{
  explicit llarp_nodedb(llarp::thread::ThreadPool *diskworker)
      : disk(diskworker)
  {
  }

  ~llarp_nodedb()
  {
    Clear();
  }

  llarp::thread::ThreadPool *disk;
  mutable llarp::util::Mutex access;  // protects entries
  std::unordered_map< llarp::RouterID, llarp::RouterContact,
                      llarp::RouterID::Hash >
      entries GUARDED_BY(access);
  fs::path nodePath;

  bool
  Remove(const llarp::RouterID &pk) LOCKS_EXCLUDED(access);

  void
  RemoveIf(std::function< bool(const llarp::RouterContact &) > filter)
      LOCKS_EXCLUDED(access);

  void
  Clear() LOCKS_EXCLUDED(access);

  bool
  Get(const llarp::RouterID &pk, llarp::RouterContact &result)
      LOCKS_EXCLUDED(access);

  bool
  Has(const llarp::RouterID &pk) LOCKS_EXCLUDED(access);

  std::string
  getRCFilePath(const llarp::RouterID &pubkey) const;

  /// insert and write to disk
  bool
  Insert(const llarp::RouterContact &rc) LOCKS_EXCLUDED(access);

  /// insert and write to disk in background
  void
  InsertAsync(llarp::RouterContact rc,
              std::shared_ptr< llarp::Logic > l             = nullptr,
              std::function< void(void) > completionHandler = nullptr);

  ssize_t
  Load(const fs::path &path);

  ssize_t
  loadSubdir(const fs::path &dir);

  bool
  loadfile(const fs::path &fpath) LOCKS_EXCLUDED(access);

  void
  visit(std::function< bool(const llarp::RouterContact &) > visit)
      LOCKS_EXCLUDED(access);

  bool
  iterate(llarp_nodedb_iter &i) LOCKS_EXCLUDED(access);

  void
  set_dir(const char *dir);

  ssize_t
  load_dir(const char *dir);
  ssize_t
  store_dir(const char *dir);

  int
  iterate_all(llarp_nodedb_iter i);

  size_t
  num_loaded() const LOCKS_EXCLUDED(access);

  bool
  select_random_exit(llarp::RouterContact &rc) LOCKS_EXCLUDED(access);

  bool
  select_random_hop(const llarp::RouterContact &prev,
                    llarp::RouterContact &result, size_t N)
      LOCKS_EXCLUDED(access);

  bool
  select_random_hop_excluding(llarp::RouterContact &result,
                              const std::set< llarp::RouterID > &exclude)
      LOCKS_EXCLUDED(access);

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
  llarp_nodedb *nodedb;
  // llarp::Logic for queue_job
  std::shared_ptr< llarp::Logic > logic;  // includes a llarp_threadpool
  llarp_threadpool *cryptoworker;
  llarp::thread::ThreadPool *diskworker;

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
  llarp_nodedb *nodedb;
  /// llarp::Logic for calling hook
  llarp::Logic *logic;
  /// disk worker threadpool
  llarp::thread::ThreadPool *diskworker;
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
