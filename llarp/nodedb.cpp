#include <nodedb.hpp>

#include <crypto/crypto.hpp>
#include <crypto/types.hpp>
#include <router_contact.hpp>
#include <util/buffer.hpp>
#include <util/encode.hpp>
#include <util/fs.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.hpp>
#include <util/thread/logic.hpp>
#include <util/thread/thread_pool.hpp>
#include <dht/kademlia.hpp>

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <utility>

static const char skiplist_subdirs[] = "0123456789abcdef";
static const std::string RC_FILE_EXT = ".signed";

llarp_nodedb::NetDBEntry::NetDBEntry(llarp::RouterContact value)
    : rc(std::move(value)), inserted(llarp::time_now_ms())
{
}

bool
llarp_nodedb::Remove(const llarp::RouterID &pk)
{
  bool removed = false;
  RemoveIf([&](const llarp::RouterContact &rc) -> bool {
    if(rc.pubkey == pk)
    {
      removed = true;
      return true;
    }
    return false;
  });
  return removed;
}

void
llarp_nodedb::Clear()
{
  llarp::util::Lock lock(access);
  entries.clear();
}

bool
llarp_nodedb::Get(const llarp::RouterID &pk, llarp::RouterContact &result)
{
  llarp::util::Lock l(access);
  auto itr = entries.find(pk);
  if(itr == entries.end())
    return false;
  result = itr->second.rc;
  return true;
}

void
KillRCJobs(const std::set< std::string > &files)
{
  for(const auto &file : files)
    fs::remove(file);
}

void
llarp_nodedb::RemoveIf(
    std::function< bool(const llarp::RouterContact &rc) > filter)
{
  std::set< std::string > files;
  {
    llarp::util::Lock l(access);
    auto itr = entries.begin();
    while(itr != entries.end())
    {
      if(filter(itr->second.rc))
      {
        files.insert(getRCFilePath(itr->second.rc.pubkey));
        itr = entries.erase(itr);
      }
      else
        ++itr;
    }
  }

  disk->addJob(std::bind(&KillRCJobs, files));
}

bool
llarp_nodedb::Has(const llarp::RouterID &pk)
{
  llarp::util::Lock lock(access);
  return entries.find(pk) != entries.end();
}

llarp::RouterContact
llarp_nodedb::FindClosestTo(const llarp::dht::Key_t &location)
{
  llarp::RouterContact rc;
  const llarp::dht::XorMetric compare(location);
  visit([&rc, compare](const auto &otherRC) -> bool {
    if(rc.pubkey.IsZero())
    {
      rc = otherRC;
      return true;
    }
    if(compare(llarp::dht::Key_t{otherRC.pubkey.as_array()},
               llarp::dht::Key_t{rc.pubkey.as_array()}))
      rc = otherRC;
    return true;
  });
  return rc;
}

std::vector< llarp::RouterContact >
llarp_nodedb::FindClosestTo(const llarp::dht::Key_t &location,
                            uint32_t numRouters)
{
  llarp::util::Lock lock(access);
  std::vector< const llarp::RouterContact * > all;

  all.reserve(entries.size());
  for(auto &entry : entries)
  {
    all.push_back(&entry.second.rc);
  }

  auto it_mid = numRouters < all.size() ? all.begin() + numRouters : all.end();
  std::partial_sort(all.begin(), it_mid, all.end(),
                    [compare = llarp::dht::XorMetric{location}](
                        auto *a, auto *b) { return compare(*a, *b); });

  std::vector< llarp::RouterContact > closest;
  closest.reserve(numRouters);
  for(auto it = all.begin(); it != it_mid; ++it)
    closest.push_back(**it);

  return closest;
}

/// skiplist directory is hex encoded first nibble
/// skiplist filename is <base32encoded>.snode.signed
std::string
llarp_nodedb::getRCFilePath(const llarp::RouterID &pubkey) const
{
  char ftmp[68] = {0};
  const char *hexname =
      llarp::HexEncode< llarp::AlignedBuffer< 32 >, decltype(ftmp) >(pubkey,
                                                                     ftmp);
  std::string hexString(hexname);
  std::string skiplistDir;

  llarp::RouterID r(pubkey);
  std::string fname = r.ToString();

  skiplistDir += hexString[0];
  fname += RC_FILE_EXT;
  fs::path filepath = nodePath / skiplistDir / fname;
  return filepath.string();
}

void
llarp_nodedb::InsertAsync(llarp::RouterContact rc,
                          std::shared_ptr< llarp::Logic > logic,
                          std::function< void(void) > completionHandler)
{
  disk->addJob([this, rc, logic, completionHandler]() {
    this->Insert(rc);
    if(logic && completionHandler)
    {
      LogicCall(logic, completionHandler);
    }
  });
}

bool
llarp_nodedb::UpdateAsyncIfNewer(llarp::RouterContact rc,
                                 std::shared_ptr< llarp::Logic > logic,
                                 std::function< void(void) > completionHandler)
{
  llarp::util::Lock lock(access);
  auto itr = entries.find(rc.pubkey);
  if(itr == entries.end() || itr->second.rc.OtherIsNewer(rc))
  {
    InsertAsync(rc, logic, completionHandler);
    return true;
  }
  if(itr != entries.end())
  {
    // insertion time is set on...insertion.  But it should be updated here
    // even if there is no insertion of a new RC, to show that the existing one
    // is not "stale"
    itr->second.inserted = llarp::time_now_ms();
  }
  return false;
}

/// insert
bool
llarp_nodedb::Insert(const llarp::RouterContact &rc)
{
  llarp::util::Lock lock(access);
  auto itr = entries.find(rc.pubkey.as_array());
  if(itr != entries.end())
    entries.erase(itr);
  entries.emplace(rc.pubkey.as_array(), rc);
  LogDebug("Added or updated RC for ", llarp::RouterID(rc.pubkey),
           " to nodedb.  Current nodedb count is: ", entries.size());
  return true;
}

ssize_t
llarp_nodedb::Load(const fs::path &path)
{
  std::error_code ec;
  if(!fs::exists(path, ec))
  {
    return -1;
  }
  ssize_t loaded = 0;

  for(const char &ch : skiplist_subdirs)
  {
    if(!ch)
      continue;
    std::string p;
    p += ch;
    fs::path sub = path / p;

    ssize_t l = loadSubdir(sub);
    if(l > 0)
      loaded += l;
  }
  m_NextSaveToDisk = llarp::time_now_ms() + m_SaveInterval;
  return loaded;
}

void
llarp_nodedb::SaveAll()
{
  std::array< byte_t, MAX_RC_SIZE > tmp;
  llarp::util::Lock lock(access);
  for(const auto &item : entries)
  {
    llarp_buffer_t buf(tmp);

    if(!item.second.rc.BEncode(&buf))
      continue;

    buf.sz              = buf.cur - buf.base;
    const auto filepath = getRCFilePath(item.second.rc.pubkey);
    auto optional_ofs   = llarp::util::OpenFileStream< std::ofstream >(
        filepath,
        std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if(!optional_ofs)
      continue;
    auto &ofs = optional_ofs.value();
    ofs.write((char *)buf.base, buf.sz);
    ofs.flush();
    ofs.close();
  }
}

bool
llarp_nodedb::ShouldSaveToDisk(llarp_time_t now) const
{
  if(now == 0s)
    now = llarp::time_now_ms();
  return m_NextSaveToDisk > 0s && m_NextSaveToDisk <= now;
}

void
llarp_nodedb::AsyncFlushToDisk()
{
  disk->addJob(std::bind(&llarp_nodedb::SaveAll, this));
  m_NextSaveToDisk = llarp::time_now_ms() + m_SaveInterval;
}

ssize_t
llarp_nodedb::loadSubdir(const fs::path &dir)
{
  ssize_t sz = 0;
  llarp::util::IterDir(dir, [&](const fs::path &f) -> bool {
    if(fs::is_regular_file(f) && loadfile(f))
      sz++;
    return true;
  });
  return sz;
}

bool
llarp_nodedb::loadfile(const fs::path &fpath)
{
  if(fpath.extension() != RC_FILE_EXT)
    return false;
  llarp::RouterContact rc;
  if(!rc.Read(fpath.string().c_str()))
  {
    llarp::LogError("failed to read file ", fpath);
    return false;
  }
  if(!rc.Verify(llarp::time_now_ms()))
  {
    llarp::LogError(fpath, " contains invalid RC");
    return false;
  }
  {
    llarp::util::Lock lock(access);
    entries.emplace(rc.pubkey.as_array(), rc);
  }
  return true;
}

void
llarp_nodedb::visit(std::function< bool(const llarp::RouterContact &) > visit)
{
  llarp::util::Lock lock(access);
  auto itr = entries.begin();
  while(itr != entries.end())
  {
    if(!visit(itr->second.rc))
      return;
    ++itr;
  }
}

void
llarp_nodedb::VisitInsertedBefore(
    std::function< void(const llarp::RouterContact &) > visit,
    llarp_time_t insertedAfter)
{
  llarp::util::Lock lock(access);
  auto itr = entries.begin();
  while(itr != entries.end())
  {
    if(itr->second.inserted < insertedAfter)
      visit(itr->second.rc);
    ++itr;
  }
}

void
llarp_nodedb::RemoveStaleRCs(const std::set< llarp::RouterID > &keep,
                             llarp_time_t cutoff)
{
  std::set< llarp::RouterID > removeStale;
  // remove stale routers
  VisitInsertedBefore(
      [&](const llarp::RouterContact &rc) {
        if(keep.find(rc.pubkey) != keep.end())
          return;
        LogInfo("removing stale router: ", llarp::RouterID(rc.pubkey));
        removeStale.insert(rc.pubkey);
      },
      cutoff);

  RemoveIf([&removeStale](const llarp::RouterContact &rc) -> bool {
    return removeStale.count(rc.pubkey) > 0;
  });
}

/*
bool
llarp_nodedb::Save()
{
  auto itr = entries.begin();
  while(itr != entries.end())
  {
    llarp::pubkey pk = itr->first;
    llarp_rc *rc= itr->second;

    itr++; // advance
  }
  return true;
}
*/

// call request hook
void
logic_threadworker_callback(void *user)
{
  auto *verify_request = static_cast< llarp_async_verify_rc * >(user);
  if(verify_request->hook)
    verify_request->hook(verify_request);
}

// write it to disk
void
disk_threadworker_setRC(llarp_async_verify_rc *verify_request)
{
  verify_request->valid = verify_request->nodedb->Insert(verify_request->rc);
  if(verify_request->logic)
    verify_request->logic->queue_job(
        {verify_request, &logic_threadworker_callback});
}

// we run the crypto verify in the crypto threadpool worker
void
crypto_threadworker_verifyrc(void *user)
{
  auto *verify_request    = static_cast< llarp_async_verify_rc * >(user);
  llarp::RouterContact rc = verify_request->rc;
  verify_request->valid   = rc.Verify(llarp::time_now_ms());
  // if it's valid we need to set it
  if(verify_request->valid && rc.IsPublicRouter())
  {
    if(verify_request->diskworker)
    {
      llarp::LogDebug("RC is valid, saving to disk");
      verify_request->diskworker->addJob(
          std::bind(&disk_threadworker_setRC, verify_request));
      return;
    }
  }
  // callback to logic thread
  verify_request->logic->queue_job(
      {verify_request, &logic_threadworker_callback});
}

void
nodedb_inform_load_rc(void *user)
{
  auto *job = static_cast< llarp_async_load_rc * >(user);
  job->hook(job);
}

void
llarp_nodedb_async_verify(struct llarp_async_verify_rc *job)
{
  job->cryptoworker->addJob(std::bind(&crypto_threadworker_verifyrc, job));
}

void
nodedb_async_load_rc(void *user)
{
  auto *job = static_cast< llarp_async_load_rc * >(user);

  auto fpath  = job->nodedb->getRCFilePath(job->pubkey);
  job->loaded = job->nodedb->loadfile(fpath);
  if(job->loaded)
  {
    job->nodedb->Get(job->pubkey, job->result);
  }
  job->logic->queue_job({job, &nodedb_inform_load_rc});
}

bool
llarp_nodedb::ensure_dir(const char *dir)
{
  fs::path path(dir);
  std::error_code ec;

  if(!fs::exists(dir, ec))
    fs::create_directory(path, ec);

  if(ec)
    return false;

  if(!fs::is_directory(path))
    return false;

  for(const char &ch : skiplist_subdirs)
  {
    // this seems to be a problem on all targets
    // perhaps cpp17::fs is just as screwed-up
    // attempting to create a folder with no name
    if(!ch)
      return true;
    std::string p;
    p += ch;
    fs::path sub = path / p;
    fs::create_directory(sub, ec);
    if(ec)
      return false;
  }
  return true;
}

ssize_t
llarp_nodedb::LoadAll()
{
  return Load(nodePath.c_str());
}

size_t
llarp_nodedb::num_loaded() const
{
  auto l = llarp::util::shared_lock(access);
  return entries.size();
}

bool
llarp_nodedb::select_random_exit(llarp::RouterContact &result)
{
  llarp::util::Lock lock(access);
  const auto sz = entries.size();
  auto itr      = entries.begin();
  if(sz < 3)
    return false;
  auto idx = llarp::randint() % sz;
  if(idx)
    std::advance(itr, idx - 1);
  while(itr != entries.end())
  {
    if(itr->second.rc.IsExit())
    {
      result = itr->second.rc;
      return true;
    }
    ++itr;
  }
  // wrap around
  itr = entries.begin();
  while(idx--)
  {
    if(itr->second.rc.IsExit())
    {
      result = itr->second.rc;
      return true;
    }
    ++itr;
  }
  return false;
}

bool
llarp_nodedb::select_random_hop(const llarp::RouterContact &prev,
                                llarp::RouterContact &result, size_t N)
{
  (void)N;
  return select_random_hop_excluding(result, {prev.pubkey});
}

bool
llarp_nodedb::select_random_hop_excluding(
    llarp::RouterContact &result, const std::set< llarp::RouterID > &exclude)
{
  llarp::util::Lock lock(access);
  /// checking for "guard" status for N = 0 is done by caller inside of
  /// pathbuilder's scope
  const size_t sz = entries.size();
  if(sz < 3)
  {
    return false;
  }

  auto itr         = entries.begin();
  const size_t pos = llarp::randint() % sz;
  std::advance(itr, pos);
  const auto start = itr;
  while(itr == entries.end())
  {
    if(exclude.count(itr->first) == 0)
    {
      if(itr->second.rc.IsPublicRouter())
      {
        result = itr->second.rc;
        return true;
      }
    }
    itr++;
  }
  itr = entries.begin();
  while(itr != start)
  {
    if(exclude.count(itr->first) == 0)
    {
      if(itr->second.rc.IsPublicRouter())
      {
        result = itr->second.rc;
        return true;
      }
    }
    ++itr;
  }
  return false;
}
