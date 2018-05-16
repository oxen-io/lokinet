#include <llarp/nodedb.h>
#include <llarp/router_contact.h>
#include <map>
#include "crypto.hpp"
#include "fs.hpp"
#include "mem.hpp"

static const char skiplist_subdirs[] = "0123456789ABCDEF";

struct llarp_nodedb {

  llarp_nodedb(struct llarp_alloc * m) : mem(m) {}
  
  llarp_alloc * mem;
  std::map<llarp::pubkey, llarp_rc *> entries;

  void Clear()
  {
    auto itr = entries.begin();
    while(itr != entries.end())
    {
      mem->free(mem, itr->second);
      itr = entries.erase(itr);
    }
  }
  
  ssize_t Load(const fs::path &path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
      return -1;
    }
    ssize_t loaded = 0;

    for (const char &ch : skiplist_subdirs) {
      fs::path sub = path / std::string(ch, 1);
      for (auto &f : fs::directory_iterator(sub)) {
        ssize_t l = loadSubdir(f);
        if (l > 0) loaded += l;
      }
    }
    return loaded;
  }

  bool loadfile(const fs::path &fpath) {
    llarp_buffer_t buff;
    FILE *f = fopen(fpath.c_str(), "rb");
    if (!f) return false;
    if (!llarp_buffer_readfile(&buff, f, mem)) {
      fclose(f);
      return false;
    }
    fclose(f);
    llarp_rc *rc = llarp::Alloc<llarp_rc>(mem);
    if (llarp_rc_bdecode(mem, rc, &buff)) {
      if (llarp_rc_verify_sig(rc)) {
        llarp::pubkey pk;
        memcpy(pk.data(), rc->pubkey, pk.size());
        entries[pk] = rc;
        return true;
      }
    }
    llarp_rc_free(rc);
    mem->free(mem, rc);
    return false;
  }

  ssize_t loadSubdir(const fs::path &dir) {
    ssize_t sz = 0;
    for (auto &path : fs::directory_iterator(dir)) {
      if (loadfile(path)) sz++;
    }
    return sz;
  }
};

extern "C" {

struct llarp_nodedb *llarp_nodedb_new(struct llarp_alloc * mem) {
  void * ptr = mem->alloc(mem, sizeof(llarp_nodedb), llarp::alignment<llarp_nodedb>());
  if(!ptr) return nullptr;
  return new (ptr) llarp_nodedb(mem);
}

void llarp_nodedb_free(struct llarp_nodedb **n) {
  if (*n)
  {
    struct llarp_alloc *mem = (*n)->mem;
    (*n)->Clear();
    (*n)->~llarp_nodedb();
    mem->free(mem, *n);
  }
  *n = nullptr;
}

bool llarp_nodedb_ensure_dir(const char *dir) {
  fs::path path(dir);
  std::error_code ec;
  if (!fs::exists(dir, ec)) fs::create_directories(path, ec);

  if (ec) return false;

  if (!fs::is_directory(path)) return false;

  for (const char &ch : skiplist_subdirs) {
    fs::path sub = path / std::string(ch, 1);
    fs::create_directory(sub, ec);
    if (ec) return false;
  }
  return true;
}

ssize_t llarp_nodedb_load_dir(struct llarp_nodedb *n, const char *dir) {
  return n->Load(dir);
}
}
