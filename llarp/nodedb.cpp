#include <llarp/crypto_async.h>
#include <llarp/nodedb.h>
#include <llarp/router_contact.h>

#include <fstream>
#include <llarp/crypto.hpp>
#include <unordered_map>
#include "buffer.hpp"
#include "encode.hpp"
#include "fs.hpp"
#include "logger.hpp"
#include "mem.hpp"

static const char skiplist_subdirs[] = "0123456789abcdef";

struct llarp_nodedb
{
  llarp_nodedb(llarp_crypto *c) : crypto(c)
  {
  }

  llarp_crypto *crypto;
  // std::map< llarp::pubkey, llarp_rc  > entries;
  std::unordered_map< llarp::PubKey, llarp_rc, llarp::PubKeyHash > entries;
  fs::path nodePath;

  void
  Clear()
  {
    auto itr = entries.begin();
    while(itr != entries.end())
    {
      llarp_rc_clear(&itr->second);
      itr = entries.erase(itr);
    }
  }

  llarp_rc *
  getRC(const llarp::PubKey &pk)
  {
    return &entries[pk];
  }

  bool
  Has(const llarp::PubKey &pk)
  {
    return entries.find(pk) != entries.end();
  }

/*
  bool
  Has(const byte_t *pk)
  {
    llarp::PubKey test(pk);
    auto itr = this->entries.begin();
    while(itr != this->entries.end())
    {
      llarp::Info("Has byte_t [", test.size(), "] vs [", itr->first.size(), "]");
      if (memcmp(test.data(), itr->first.data(), 32) == 0) {
        llarp::Info("Match");
      }
      itr++;
    }
    return entries.find(pk) != entries.end();
  }
*/

  bool
  pubKeyExists(llarp_rc *rc)
  {
    // extract pk from rc
    llarp::PubKey pk = rc->pubkey;
    // return true if we found before end
    return entries.find(pk) != entries.end();
  }

  bool
  check(llarp_rc *rc)
  {
    if(!pubKeyExists(rc))
    {
      // we don't have it
      return false;
    }
    llarp::PubKey pk = rc->pubkey;

    // TODO: zero out any fields you don't want to compare

    // serialize both and memcmp
    byte_t nodetmp[MAX_RC_SIZE];
    auto nodebuf = llarp::StackBuffer< decltype(nodetmp) >(nodetmp);
    if(llarp_rc_bencode(&entries[pk], &nodebuf))
    {
      byte_t paramtmp[MAX_RC_SIZE];
      auto parambuf = llarp::StackBuffer< decltype(paramtmp) >(paramtmp);
      if(llarp_rc_bencode(rc, &parambuf))
      {
        if(nodebuf.sz == parambuf.sz)
          return memcmp(&parambuf, &nodebuf, parambuf.sz) == 0;
      }
    }
    return false;
  }

  std::string
  getRCFilePath(const byte_t *pubkey)
  {
    char ftmp[68] = {0};
    const char *hexname =
        llarp::HexEncode< llarp::PubKey, decltype(ftmp) >(pubkey, ftmp);
    std::string hexString(hexname);
    std::string filepath = nodePath;
    filepath.append(PATH_SEP);
    filepath.append(&hexString[hexString.length() - 1]);
    filepath.append(PATH_SEP);
    filepath.append(hexname);
    filepath.append(".signed");
    return filepath;
  }

  bool
  setRC(llarp_rc *rc)
  {
    byte_t tmp[MAX_RC_SIZE];
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);

    // extract pk from rc
    llarp::PubKey pk = rc->pubkey;

    // set local db entry to have a copy we own
    llarp_rc entry;
    llarp::Zero(&entry, sizeof(entry));
    llarp_rc_copy(&entry, rc);
    entries[pk] = entry;

    if(llarp_rc_bencode(&entry, &buf))
    {
      buf.sz        = buf.cur - buf.base;
      auto filepath = getRCFilePath(pk);
      llarp::Debug("saving RC.pubkey ", filepath);
      std::ofstream ofs(
          filepath,
          std::ofstream::out & std::ofstream::binary & std::ofstream::trunc);
      ofs.write((char *)buf.base, buf.sz);
      ofs.close();
      if(!ofs)
      {
        llarp::Error("Failed to write: ", filepath);
        return false;
      }
      llarp::Debug("saved RC.pubkey: ", filepath);
      return true;
    }
    return false;
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
    fs::directory_iterator i(dir);
    auto itr = i.begin();
    while(itr != itr.end())
    {
      if (fs::is_regular_file(itr->symlink_status()) && loadfile(*itr))
        sz++;

      ++itr;
    }
    return sz;
  }

  bool
  loadfile(const fs::path &fpath)
  {
#if __APPLE__ && __MACH__
    // skip .DS_Store files
    if (strstr(fpath.c_str(), ".DS_Store") != 0) {
      return false;
    }
#endif
    llarp_rc *rc = llarp_rc_read(fpath.c_str());
    if (!rc)
    {
      llarp::Error("Signature read failed", fpath);
      return false;
    }
    if(!llarp_rc_verify_sig(crypto, rc))
    {
      llarp::Error("Signature verify failed", fpath);
      return false;
    }
    llarp::PubKey pk(rc->pubkey);
    entries[pk] = *rc;
    return true;
  }

  bool iterate(struct llarp_nodedb_iter i) {
    i.index = 0;
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
  verify_request->valid = verify_request->nodedb->setRC(&verify_request->rc);
  llarp_logic_queue_job(verify_request->logic,
                        {verify_request, &logic_threadworker_callback});
}

// we run the crypto verify in the crypto threadpool worker
void
crypto_threadworker_verifyrc(void *user)
{
  llarp_async_verify_rc *verify_request =
      static_cast< llarp_async_verify_rc * >(user);
  verify_request->valid =
      llarp_rc_verify_sig(verify_request->nodedb->crypto, &verify_request->rc);
  // if it's valid we need to set it
  if(verify_request->valid)
  {
    llarp::Debug("RC is valid, saving to disk");
    llarp_threadpool_queue_job(verify_request->diskworker,
                               {verify_request, &disk_threadworker_setRC});
  }
  else
  {
    // callback to logic thread
    llarp::Warn("RC is not valid, can't save to disk");
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
    llarp_rc_copy(&job->rc, job->nodedb->getRC(job->pubkey));
  }
  llarp_logic_queue_job(job->logic, {job, &nodedb_inform_load_rc});
}

extern "C" {
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
llarp_nodedb_load_dir(struct llarp_nodedb *n, const char *dir)
{
  std::error_code ec;
  if(!fs::exists(dir, ec))
  {
    return -1;
  }
  n->nodePath = dir;
  return n->Load(dir);
}

bool
llarp_nodedb_put_rc(struct llarp_nodedb *n, struct llarp_rc *rc) {
  return n->setRC(rc);
}

int
llarp_nodedb_iterate_all(struct llarp_nodedb *n, struct llarp_nodedb_iter i) {
  n->iterate(i);
  return n->entries.size();
}

void
llarp_nodedb_async_verify(struct llarp_async_verify_rc *job)
{
  // switch to crypto threadpool and continue with
  // crypto_threadworker_verifyrc
  llarp_threadpool_queue_job(job->cryptoworker,
                             {job, &crypto_threadworker_verifyrc});
}

void
llarp_nodedb_async_load_rc(struct llarp_async_load_rc *job)
{
  // call in the disk io thread so we don't bog down the others
  llarp_threadpool_queue_job(job->diskworker, {job, &nodedb_async_load_rc});
}

struct llarp_rc *
llarp_nodedb_get_rc(struct llarp_nodedb *n, const byte_t *pk)
{
  //llarp::Info("llarp_nodedb_get_rc [", pk, "]");
  if(n->Has(pk))
    return n->getRC(pk);
  else
    return nullptr;
}

size_t
llarp_nodedb_num_loaded(struct llarp_nodedb *n)
{
  return n->entries.size();
}

void
llarp_nodedb_select_random_hop(struct llarp_nodedb *n, struct llarp_rc *prev,
                               struct llarp_rc *result, size_t N)
{
  /// TODO: check for "guard" status for N = 0?
  auto sz = n->entries.size();

  if(prev)
  {
    do
    {
      auto itr = n->entries.begin();
      if(sz > 1)
      {
        auto idx = rand() % (sz - 1);
        std::advance(itr, idx);
      }
      if(memcmp(prev->pubkey, itr->second.pubkey, PUBKEYSIZE) == 0)
        continue;
      llarp_rc_copy(result, &itr->second);
      return;
    } while(true);
  }
  else
  {
    auto itr = n->entries.begin();
    if(sz > 1)
    {
      auto idx = rand() % (sz - 1);
      std::advance(itr, idx);
    }
    llarp_rc_copy(result, &itr->second);
  }
}

}  // end extern
