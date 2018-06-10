#include <llarp/nodedb.h>
#include <llarp/crypto.hpp>
#include <llarp/router_contact.h>
#include <fstream>
#include <unordered_map>
#include "buffer.hpp"
#include "fs.hpp"
#include "mem.hpp"

static const char skiplist_subdirs[] = "0123456789ABCDEF";

struct llarp_nodedb
{
  llarp_nodedb(llarp_crypto *c) : crypto(c)
  {
  }

  llarp_crypto *crypto;
  std::unordered_map< llarp::PubKey, llarp_rc * , llarp::PubKeyHash> entries;

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
        llarp::PubKey pk(&rc->pubkey[0]);
        entries[pk] = rc;
        return true;
      }
    }
    llarp_rc_free(rc);
    delete rc;
    return false;
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

void
llarp_nodedb_async_verify(struct llarp_nodedb *nodedb,
                          struct llarp_logic *logic,
                          struct llarp_crypto *crypto,
                          struct llarp_threadpool *cryptoworker,
                          struct llarp_threadpool *diskworker,
                          struct llarp_async_verify_rc *job)
{
}

bool
llarp_nodedb_find_rc(struct llarp_nodedb *nodedb, struct llarp_rc *dst,
                     llarp_pubkey_t k)
{
  return false;
}
}
