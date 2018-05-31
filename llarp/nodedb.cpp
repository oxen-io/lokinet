#include <llarp/nodedb.h>
#include <llarp/router_contact.h>
#include <llarp/crypto_async.h>
#include <llarp/threadpool.h>

#include <fstream>
#include <map>
#include "buffer.hpp"
#include "crypto.hpp"
#include "fs.hpp"
#include "mem.hpp"
#include "encode.hpp"
#include "logger.hpp"

// probably used for more than verify tbh
struct llarp_async_verify_job_context
{
    struct llarp_logic *logic;
    struct llarp_crypto *crypto;
    struct llarp_threadpool *cryptoworker;
    struct llarp_threadpool *diskworker;
};

static const char skiplist_subdirs[] = "0123456789ABCDEF";

static void on_crypt_verify_rc(rc_async_verify *job)
{
  if (job->result) {
    // set up disk request
    // how do we get our diskworker?
  } else {
    // it's not valid, don't update db
    // send back to logic thread

    // make generic job based on previous job
    //llarp_thread_job job = {.user = job->user, .work = &inform_verify_rc};
    //llarp_logic_queue_job(job->context->logic, job);
  }
  // TODO: is there any deallocation we need to do
  delete (llarp_async_verify_job_context*)job->context; // clean up our temp context created in verify_rc
  delete job; // we're done with the rc_async_verify
}

void verify_rc(void *user)
{
  llarp_async_verify_rc *verify_request =
    static_cast< llarp_async_verify_rc * >(user);
  // transfer context
  // FIXME: move this allocation to more a long term home?
  llarp_async_rc *async_rc_context = llarp_async_rc_new(
    verify_request->context->crypto, verify_request->context->logic,
    verify_request->context->cryptoworker);
  // set up request
  rc_async_verify *async_rc_request = new rc_async_verify;
  // rc_call_async_verify will set up context, rc
  // user?
  async_rc_request->result = false; // just initialize it to something secure
  async_rc_request->hook = &on_crypt_verify_rc;

  rc_call_async_verify(async_rc_context, async_rc_request,
    &verify_request->rc);
  // crypto verify
  // if success write to disk
  //verify_request->context->crypto
  //llarp_thread_job job = {.user = user, .work = &inform_keygen};
  //llarp_logic_queue_job(keygen->iwp->logic, job);
}

struct llarp_nodedb
{
  llarp_nodedb(llarp_crypto *c) : crypto(c)
  {
  }

  llarp_crypto *crypto;
  std::map< llarp::pubkey, llarp_rc * > entries;

  void
  Clear()
  {
    auto itr = entries.begin();
    while(itr != entries.end())
    {
      delete itr->second;
      itr = entries.erase(itr);
    }
  }

  inline llarp::pubkey getPubKeyFromRC(llarp_rc *rc)
  {
    llarp::pubkey pk;
    memcpy(pk.data(), rc->pubkey, pk.size());
    return pk;
  }

  llarp_rc *getRC(llarp::pubkey pk)
  {
    return entries[pk];
  }

  bool pubKeyExists(llarp_rc *rc)
  {
    // extract pk from rc
    llarp::pubkey pk = getPubKeyFromRC(rc);
    // return true if we found before end
    return entries.find(pk) != entries.end();
  }

  bool check(llarp_rc *rc)
  {
    if (!pubKeyExists(rc))
    {
      // we don't have it
      return false;
    }
    llarp::pubkey pk = getPubKeyFromRC(rc);

    // TODO: zero out any fields you don't want to compare

    // serialize both and memcmp
    byte_t nodetmp[MAX_RC_SIZE];
    auto nodebuf = llarp::StackBuffer< decltype(nodetmp) >(nodetmp);
    if (llarp_rc_bencode(entries[pk], &nodebuf))
    {
      byte_t paramtmp[MAX_RC_SIZE];
      auto parambuf = llarp::StackBuffer< decltype(paramtmp) >(paramtmp);
      if (llarp_rc_bencode(rc, &parambuf))
      {
        if (memcmp(&parambuf, &nodebuf, MAX_RC_SIZE) == 0)
        {
          return true;
        }
      }
    }
    return false;
  }

  bool setRC(llarp_rc *rc) {
    byte_t tmp[MAX_RC_SIZE];
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);

    // extract pk from rc
    llarp::pubkey pk = getPubKeyFromRC(rc);

    // set local db
    entries[pk] = rc;

    if (llarp_rc_bencode(rc, &buf))
    {
      // write buf to disk
      //auto filename = hexStr(pk.data(), sizeof(pk)) + ".rc";
      char ftmp[68] = {0};
      const char *hexname =
        llarp::HexEncode< llarp::pubkey, decltype(ftmp) >(pk, ftmp);
      std::string filename(hexname);
      // FIXME: path?
      printf("filename[%s]\n", filename.c_str());
      std::ofstream ofs (filename, std::ofstream::out & std::ofstream::binary & std::ofstream::trunc);
      ofs.write((char *)buf.base, buf.sz);
      ofs.close();
      if (!ofs)
      {
        llarp::Error(__FILE__, "Failed to write", filename);
        return false;
      }
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
      for(auto &f : fs::directory_iterator(sub))
      {
        ssize_t l = loadSubdir(f);
        if(l > 0)
          loaded += l;
      }
    }
    return loaded;
  }

  ssize_t
  loadSubdir(const fs::path &dir)
  {
    ssize_t sz = 0;
    for(auto &path : fs::directory_iterator(dir))
    {
      if(loadfile(path))
        sz++;
    }
    return sz;
  }

  bool
  loadfile(const fs::path &fpath)
  {
    std::ifstream f(fpath, std::ios::binary);
    if(!f.is_open())
      return false;

    byte_t tmp[MAX_RC_SIZE];

    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    f.seekg(0, std::ios::end);
    size_t sz = f.tellg();
    f.seekg(0, std::ios::beg);

    if(sz > buf.sz)
      return false;

    // TODO: error checking
    f.read((char *)buf.base, sz);
    buf.sz = sz;

    llarp_rc *rc = new llarp_rc;
    llarp::Zero(rc, sizeof(llarp_rc));
    if(llarp_rc_bdecode(rc, &buf))
    {
      if(llarp_rc_verify_sig(crypto, rc))
      {
        llarp::pubkey pk;
        memcpy(pk.data(), rc->pubkey, pk.size());
        entries[pk] = rc;
        return true;
      }
    }
    llarp_rc_free(rc);
    delete rc;
    return false;
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
  return n->Load(dir);
}

/// allocate verify job context
struct llarp_async_verify_job_context*
llarp_async_verify_job_new(struct llarp_threadpool *cryptoworker,
  struct llarp_threadpool *diskworker) {
  llarp_async_verify_job_context *context = new llarp_async_verify_job_context;
  if (context)
  {
    context->cryptoworker = cryptoworker;
    context->diskworker = diskworker;
  }
  return context;
}

void
llarp_async_verify_job_free(struct llarp_async_verify_job_context *context) {
  delete context;
}

void
llarp_nodedb_async_verify(struct llarp_nodedb *nodedb,
                          struct llarp_logic *logic,
                          struct llarp_crypto *crypto,
                          struct llarp_threadpool *cryptoworker,
                          struct llarp_threadpool *diskworker,
                          struct llarp_async_verify_rc *job)
{
  printf("llarp_nodedb_async_verify\n");
  // set up context
  llarp_async_verify_job_context *context = llarp_async_verify_job_new(
    cryptoworker, diskworker);
  // set up anything we need (in job)
  job->context = context;
  // queue the crypto check
  llarp_threadpool_queue_job(cryptoworker, { job, &verify_rc });
}
}
