#include <llarp/nodedb.h>
#include <llarp/router_contact.h>
#include <fstream>
#include <map>
#include "buffer.hpp"
#include "crypto.hpp"
#include "fs.hpp"
#include "mem.hpp"
#include "logger.hpp"

constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

std::string hexStr(unsigned char *data, int len)
{
  std::string s(len * 2, ' ');
  for (int i = 0; i < len; ++i) {
    s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
    s[2 * i + 1] = hexmap[data[i] & 0x0F];
  }
  return s;
}

static const char skiplist_subdirs[] = "0123456789ABCDEF";

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

  inline llarp::pubkey getPubKeyFromRC(llarp_rc *rc) {
    llarp::pubkey pk;
    memcpy(pk.data(), rc->pubkey, pk.size());
    return pk;
  }

  llarp_rc *getRC(llarp::pubkey pk) {
    return entries[pk];
  }

  bool pubKeyExists(llarp_rc *rc) {
    // extract pk from rc
    llarp::pubkey pk = getPubKeyFromRC(rc);
    // return true if we found before end
    return entries.find(pk) != entries.end();
  }

  bool check(llarp_rc *rc) {
    if (!pubKeyExists(rc)) {
      // we don't have it
      return false;
    }
    llarp::pubkey pk = getPubKeyFromRC(rc);

    // TODO: zero out any fields you don't want to compare

    // serialize both and memcmp
    byte_t nodetmp[MAX_RC_SIZE];
    auto nodebuf = llarp::StackBuffer< decltype(nodetmp) >(nodetmp);
    if (llarp_rc_bencode(entries[pk], &nodebuf)) {
      byte_t paramtmp[MAX_RC_SIZE];
      auto parambuf = llarp::StackBuffer< decltype(paramtmp) >(paramtmp);
      if (llarp_rc_bencode(rc, &parambuf)) {
        if (memcmp(&parambuf, &nodebuf, MAX_RC_SIZE) == 0) {
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

    if (llarp_rc_bencode(rc, &buf)) {
      // write buf to disk
      auto filename = hexStr(pk.data(), sizeof(pk)) + ".rc";
      // FIXME: path?
      printf("filename[%s]\n", filename.c_str());
      std::ofstream ofs (filename, std::ofstream::out & std::ofstream::binary & std::ofstream::trunc);
      ofs.write((char *)buf.base, buf.sz);
      ofs.close();
      if (!ofs) {
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
}
