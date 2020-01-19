#ifndef LLARP_NODEDB_HPP
#define LLARP_NODEDB_HPP

#include <router_contact.hpp>
#include <router_id.hpp>
#include <dht/key.hpp>
#include <util/common.hpp>
#include <util/fs.hpp>
#include <util/thread/threading.hpp>

#include <absl/base/thread_annotations.h>

#include <set>
#include <utility>

/**
 * nodedb.hpp
 *
 * persistent storage API for router contacts
 */

struct llarp_threadpool;

namespace llarp
{
  class Logic;
  namespace service
  {
    struct Address;
  }
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
  explicit llarp_nodedb(std::shared_ptr< llarp::thread::ThreadPool > diskworker,
                        const std::string rootdir)
      : disk(std::move(diskworker)), nodePath(rootdir)

  {
  }

  ~llarp_nodedb()
  {
    Clear();
  }

  std::shared_ptr< llarp::thread::ThreadPool > disk;
  mutable llarp::util::Mutex access;  // protects entries
  /// time for next save to disk event, 0 if never happened
  llarp_time_t m_NextSaveToDisk = 0;
  /// how often to save to disk
  const llarp_time_t m_SaveInterval = 60 * 5 * 1000;

  struct NetDBEntry
  {
    const llarp::RouterContact rc;
    llarp_time_t inserted;

    NetDBEntry(llarp::RouterContact data);
  };

  using NetDBMap_t =
      std::unordered_map< llarp::RouterID, NetDBEntry, llarp::RouterID::Hash >;

  NetDBMap_t entries GUARDED_BY(access);
  fs::path nodePath;

  /// return true if we should save our nodedb to disk
  bool
  ShouldSaveToDisk(llarp_time_t now = 0) const;

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

  /// insert without writing to disk
  bool
  Insert(const llarp::RouterContact &rc) LOCKS_EXCLUDED(access);

  /// invokes Insert() asynchronously with an optional completion
  /// callback
  void
  InsertAsync(llarp::RouterContact rc,
              std::shared_ptr< llarp::Logic > l             = nullptr,
              std::function< void(void) > completionHandler = nullptr);

  /// update rc if newer
  /// return true if we started to put this rc in the database
  /// retur false if not newer
  bool
  UpdateAsyncIfNewer(llarp::RouterContact rc,
                     std::shared_ptr< llarp::Logic > l             = nullptr,
                     std::function< void(void) > completionHandler = nullptr)
      LOCKS_EXCLUDED(access);

  ssize_t
  Load(const fs::path &path);

  ssize_t
  loadSubdir(const fs::path &dir);
  /// save all entries to disk async
  void
  AsyncFlushToDisk();

  bool
  loadfile(const fs::path &fpath) LOCKS_EXCLUDED(access);

  void
  visit(std::function< bool(const llarp::RouterContact &) > visit)
      LOCKS_EXCLUDED(access);

  void
  set_dir(const char *dir);

  ssize_t
  LoadAll();

  ssize_t
  store_dir(const char *dir);

  /// visit all entries inserted into nodedb cache before a timestamp
  void
  VisitInsertedBefore(std::function< void(const llarp::RouterContact &) > visit,
                      llarp_time_t insertedAfter) LOCKS_EXCLUDED(access);

  void
  RemoveStaleRCs(const std::set< llarp::RouterID > &keep, llarp_time_t cutoff);

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

  llarp::RouterContact
  FindClosestToAddress(const llarp::dht::Key_t &addr)
      LOCKS_EXCLUDED(access);

  static bool
  ensure_dir(const char *dir);

  void
  SaveAll() LOCKS_EXCLUDED(access);
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
  std::shared_ptr< llarp::Logic > logic;
  std::shared_ptr< llarp::thread::ThreadPool > cryptoworker;
  std::shared_ptr< llarp::thread::ThreadPool > diskworker;

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
