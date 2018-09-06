#include <llarp/crypto_async.h>
#include <llarp/nodedb.hpp>
#include <llarp/router_contact.hpp>

#include <fstream>
#include <llarp/crypto.hpp>
#include <unordered_map>
#include "buffer.hpp"
#include "encode.hpp"
#include "fs.hpp"
#include "logger.hpp"
#include "mem.hpp"

static const char skiplist_subdirs[] = "0123456789abcdef";
static const std::string RC_FILE_EXT = ".signed";

struct llarp_nodedb
{
  llarp_nodedb(llarp_crypto *c) : crypto(c)
  {
  }

  llarp_crypto *crypto;
  // std::map< llarp::pubkey, llarp_rc  > entries;
  llarp::util::Mutex access;
  std::unordered_map< llarp::PubKey, llarp::RouterContact, llarp::PubKey::Hash >
      entries;
  fs::path nodePath;

  void
  Clear()
  {
    llarp::util::Lock lock(access);
    entries.clear();
  }

  bool
  Get(const llarp::PubKey &pk, llarp::RouterContact &result)
  {
    llarp::util::Lock lock(access);
    auto itr = entries.find(pk);
    if(itr == entries.end())
      return false;
    result = itr->second;
    return true;
  }

  bool
  Has(const llarp::PubKey &pk)
  {
    llarp::util::Lock lock(access);
    return entries.find(pk) != entries.end();
  }

  std::string
  getRCFilePath(const byte_t *pubkey) const
  {
    char ftmp[68] = {0};
    const char *hexname =
        llarp::HexEncode< llarp::PubKey, decltype(ftmp) >(pubkey, ftmp);
    std::string hexString(hexname);
    std::string skiplistDir;
    skiplistDir += hexString[hexString.length() - 1];
    hexString += RC_FILE_EXT;
    fs::path filepath = nodePath / skiplistDir / hexString;
    return filepath.string();
  }

  /// insert and write to disk
  bool
  Insert(const llarp::RouterContact &rc)
  {
    byte_t tmp[MAX_RC_SIZE];
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    {
      llarp::util::Lock lock(access);
      entries.insert(std::make_pair(rc.pubkey, rc));
    }
    if(!rc.BEncode(&buf))
      return false;

    buf.sz        = buf.cur - buf.base;
    auto filepath = getRCFilePath(rc.pubkey);
    llarp::LogDebug("saving RC.pubkey ", filepath);
    std::ofstream ofs(
        filepath,
        std::ofstream::out & std::ofstream::binary & std::ofstream::trunc);
    ofs.write((char *)buf.base, buf.sz);
    ofs.close();
    if(!ofs)
    {
      llarp::LogError("Failed to write: ", filepath);
      return false;
    }
    llarp::LogDebug("saved RC.pubkey: ", filepath);
    return true;
  }

  ssize_t
  Load(const fs::path &path)
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
    return loaded;
  }

  ssize_t
  loadSubdir(const fs::path &dir)
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
  loadfile(const fs::path &fpath)
  {
    if(fpath.extension() != RC_FILE_EXT)
      return false;
    llarp::RouterContact rc;

    if(!rc.Read(fpath.string().c_str()))
    {
      llarp::LogError("failed to read file ", fpath);
      return false;
    }
    if(!rc.VerifySignature(crypto))
    {
      llarp::LogError("Signature verify failed", fpath);
      return false;
    }
    {
      llarp::util::Lock lock(access);
      entries.insert(std::make_pair(rc.pubkey, rc));
    }
    return true;
  }

  bool
  iterate(struct llarp_nodedb_iter i)
  {
    i.index = 0;
    llarp::util::Lock lock(access);
    auto itr = entries.begin();
    while(itr != entries.end())
    {
      i.rc = &itr->second;
      i.visit(&i);

      // advance
      i.index++;
      itr++;
    }
    return true;
  }

  /*
  bool Save()
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
};

// call request hook
void
logic_threadworker_callback(void *user)
{
  llarp_async_verify_rc *verify_request =
      static_cast< llarp_async_verify_rc * >(user);
  verify_request->hook(verify_request);
}

// write it to disk
void
disk_threadworker_setRC(void *user)
{
  llarp_async_verify_rc *verify_request =
      static_cast< llarp_async_verify_rc * >(user);
  verify_request->valid = verify_request->nodedb->Insert(verify_request->rc);
  if(verify_request->logic)
    llarp_logic_queue_job(verify_request->logic,
                          {verify_request, &logic_threadworker_callback});
}

// we run the crypto verify in the crypto threadpool worker
void
crypto_threadworker_verifyrc(void *user)
{
  llarp_async_verify_rc *verify_request =
      static_cast< llarp_async_verify_rc * >(user);
  llarp::RouterContact rc = verify_request->rc;
  verify_request->valid   = rc.VerifySignature(verify_request->nodedb->crypto);
  // if it's valid we need to set it
  if(verify_request->valid && rc.IsPublicRouter())
  {
    llarp::LogDebug("RC is valid, saving to disk");
    llarp_threadpool_queue_job(verify_request->diskworker,
                               {verify_request, &disk_threadworker_setRC});
  }
  else
  {
    // callback to logic thread
    if(!verify_request->valid)
      llarp::LogWarn("RC is not valid, can't save to disk");
    llarp_logic_queue_job(verify_request->logic,
                          {verify_request, &logic_threadworker_callback});
  }
}

void
nodedb_inform_load_rc(void *user)
{
  llarp_async_load_rc *job = static_cast< llarp_async_load_rc * >(user);
  job->hook(job);
}

void
nodedb_async_load_rc(void *user)
{
  llarp_async_load_rc *job = static_cast< llarp_async_load_rc * >(user);

  auto fpath  = job->nodedb->getRCFilePath(job->pubkey);
  job->loaded = job->nodedb->loadfile(fpath);
  if(job->loaded)
  {
    job->nodedb->Get(job->pubkey, job->result);
  }
  llarp_logic_queue_job(job->logic, {job, &nodedb_inform_load_rc});
}

struct llarp_nodedb *
llarp_nodedb_new(struct llarp_crypto *crypto)
{
  return new llarp_nodedb(crypto);
}

void
llarp_nodedb_free(struct llarp_nodedb **n)
{
  if(*n)
  {
    auto i = *n;
    *n     = nullptr;
    i->Clear();
    delete i;
  }
}

bool
llarp_nodedb_put_rc(struct llarp_nodedb *n, const llarp::RouterContact &rc)
{
  return n->Insert(rc);
}

bool
llarp_nodedb_ensure_dir(const char *dir)
{
  fs::path path(dir);
  std::error_code ec;

  if(!fs::exists(dir, ec))
    fs::create_directories(path, ec);

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

void
llarp_nodedb_set_dir(struct llarp_nodedb *n, const char *dir)
{
  n->nodePath = dir;
}

ssize_t
llarp_nodedb_load_dir(struct llarp_nodedb *n, const char *dir)
{
  std::error_code ec;
  if(!fs::exists(dir, ec))
  {
    return -1;
  }
  llarp_nodedb_set_dir(n, dir);
  return n->Load(dir);
}

int
llarp_nodedb_iterate_all(struct llarp_nodedb *n, struct llarp_nodedb_iter i)
{
  n->iterate(i);
  return n->entries.size();
}

/// maybe rename to verify_and_set
void
llarp_nodedb_async_verify(struct llarp_async_verify_rc *job)
{
  // switch to crypto threadpool and continue with
  // crypto_threadworker_verifyrc
  llarp_threadpool_queue_job(job->cryptoworker,
                             {job, &crypto_threadworker_verifyrc});
}

// disabled for now
/*
void
llarp_nodedb_async_load_rc(struct llarp_async_load_rc *job)
{
  // call in the disk io thread so we don't bog down the others
  llarp_threadpool_queue_job(job->diskworker, {job, &nodedb_async_load_rc});
}
*/

bool
llarp_nodedb_get_rc(struct llarp_nodedb *n, const llarp::RouterID &pk,
                    llarp::RouterContact &result)
{
  // llarp::LogInfo("llarp_nodedb_get_rc [", pk, "]");
  return n->Get(pk, result);
}

size_t
llarp_nodedb_num_loaded(struct llarp_nodedb *n)
{
  return n->entries.size();
}

bool
llarp_nodedb_select_random_hop(struct llarp_nodedb *n,
                               const llarp::RouterContact &prev,
                               llarp::RouterContact &result, size_t N)
{
  /// checking for "guard" status for N = 0 is done by caller inside of
  /// pathbuilder's scope
  auto sz = n->entries.size();
  if(sz < 3)
    return false;
  size_t tries = 5;
  if(N)
  {
    do
    {
      auto itr = n->entries.begin();
      if(sz > 1)
      {
        auto idx = llarp_randint() % sz;
        if(idx)
          std::advance(itr, idx - 1);
      }
      if(prev.pubkey == itr->second.pubkey)
      {
        if(tries--)
          continue;
        return false;
      }
      if(itr->second.addrs.size())
      {
        result = itr->second;
        return true;
      }
    } while(tries--);
    return false;
  }
  else
  {
    auto itr = n->entries.begin();
    if(sz > 1)
    {
      auto idx = llarp_randint() % sz;
      if(idx)
        std::advance(itr, idx - 1);
    }
    result = itr->second;
    return true;
  }
}
