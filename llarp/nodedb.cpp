#include "nodedb.hpp"

#include "crypto/crypto.hpp"
#include "crypto/types.hpp"
#include "router_contact.hpp"
#include "util/buffer.hpp"
#include "util/fs.hpp"
#include "util/logging/logger.hpp"
#include "util/mem.hpp"
#include "util/str.hpp"
#include "dht/kademlia.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <utility>

static const char skiplist_subdirs[] = "0123456789abcdef";
static const std::string RC_FILE_EXT = ".signed";

namespace llarp
{
  NodeDB::Entry::Entry(RouterContact value) : rc(std::move(value)), insertedAt(llarp::time_now_ms())
  {}

  static void
  EnsureSkiplist(fs::path nodedbDir)
  {
    if (not fs::exists(nodedbDir))
    {
      // if the old 'netdb' directory exists, move it to this one
      fs::path parent = nodedbDir.parent_path();
      fs::path old = parent / "netdb";
      if (fs::exists(old))
        fs::rename(old, nodedbDir);
      else
        fs::create_directory(nodedbDir);
    }

    if (not fs::is_directory(nodedbDir))
      throw std::runtime_error(llarp::stringify("nodedb ", nodedbDir, " is not a directory"));

    for (const char& ch : skiplist_subdirs)
    {
      // this seems to be a problem on all targets
      // perhaps cpp17::fs is just as screwed-up
      // attempting to create a folder with no name
      // what does this mean...?
      if (!ch)
        continue;

      fs::path sub = nodedbDir / std::string(&ch, 1);
      fs::create_directory(sub);
    }
  }

  constexpr auto FlushInterval = 5min;

  NodeDB::NodeDB(fs::path root, std::function<void(std::function<void()>)> diskCaller)
      : m_Root{std::move(root)}
      , disk(std::move(diskCaller))
      , m_NextFlushAt{time_now_ms() + FlushInterval}
  {
    EnsureSkiplist(m_Root);
  }
  NodeDB::NodeDB() : m_Root{}, disk{[](auto) {}}, m_NextFlushAt{0s}
  {}

  void
  NodeDB::Tick(llarp_time_t now)
  {
    if (m_NextFlushAt == 0s)
      return;

    if (now > m_NextFlushAt)
    {
      m_NextFlushAt += FlushInterval;
      // make copy of all rcs
      std::vector<RouterContact> copy;
      for (const auto& item : m_Entries)
        copy.push_back(item.second.rc);
      // flush them to disk in one big job
      // TODO: split this up? idk maybe some day...
      disk([this, data = std::move(copy)]() {
        for (const auto& rc : data)
        {
          rc.Write(GetPathForPubkey(rc.pubkey));
        }
      });
    }
  }

  fs::path
  NodeDB::GetPathForPubkey(RouterID pubkey) const
  {
    std::string hexString = oxenmq::to_hex(pubkey.begin(), pubkey.end());
    std::string skiplistDir;

    const llarp::RouterID r{pubkey};
    std::string fname = r.ToString();

    skiplistDir += hexString[0];
    fname += RC_FILE_EXT;
    return m_Root / skiplistDir / fname;
  }

  void
  NodeDB::LoadFromDisk()
  {
    if (m_Root.empty())
      return;

    for (const char& ch : skiplist_subdirs)
    {
      if (!ch)
        continue;
      std::string p;
      p += ch;
      fs::path sub = m_Root / p;

      llarp::util::IterDir(sub, [&](const fs::path& f) -> bool {
        if (fs::is_regular_file(f) and f.extension() == RC_FILE_EXT)
        {
          RouterContact rc{};
          if (rc.Read(f) and rc.Verify(time_now_ms()))
            m_Entries.emplace(rc.pubkey, rc);
        }
        return true;
      });
    }
  }

  void
  NodeDB::SaveToDisk() const
  {
    if (m_Root.empty())
      return;

    for (const auto& item : m_Entries)
    {
      item.second.rc.Write(GetPathForPubkey(item.first));
    }
  }

  bool
  NodeDB::Has(RouterID pk) const
  {
    util::NullLock lock{m_Access};
    return m_Entries.find(pk) != m_Entries.end();
  }

  std::optional<RouterContact>
  NodeDB::Get(RouterID pk) const
  {
    util::NullLock lock{m_Access};
    const auto itr = m_Entries.find(pk);
    if (itr == m_Entries.end())
      return std::nullopt;
    return itr->second.rc;
  }

  void
  NodeDB::Remove(RouterID pk)
  {
    util::NullLock lock{m_Access};
    m_Entries.erase(pk);
    AsyncRemoveManyFromDisk({pk});
  }

  void
  NodeDB::RemoveStaleRCs(std::unordered_set<RouterID> keep, llarp_time_t cutoff)
  {
    util::NullLock lock{m_Access};
    std::unordered_set<RouterID> removed;
    auto itr = m_Entries.begin();
    while (itr != m_Entries.end())
    {
      if (itr->second.insertedAt < cutoff and keep.count(itr->second.rc.pubkey) == 0)
      {
        removed.insert(itr->second.rc.pubkey);
        itr = m_Entries.erase(itr);
      }
      else
        ++itr;
    }
    if (not removed.empty())
      AsyncRemoveManyFromDisk(std::move(removed));
  }

  void
  NodeDB::Put(RouterContact rc)
  {
    util::NullLock lock{m_Access};
    m_Entries.erase(rc.pubkey);
    m_Entries.emplace(rc.pubkey, rc);
  }

  size_t
  NodeDB::NumLoaded() const
  {
    util::NullLock lock{m_Access};
    return m_Entries.size();
  }

  void
  NodeDB::PutIfNewer(RouterContact rc)
  {
    util::NullLock lock{m_Access};
    auto itr = m_Entries.find(rc.pubkey);
    if (itr == m_Entries.end() or itr->second.rc.OtherIsNewer(rc))
    {
      // delete if existing
      if (itr != m_Entries.end())
        m_Entries.erase(itr);
      // add new entry
      m_Entries.emplace(rc.pubkey, rc);
    }
  }

  void
  NodeDB::AsyncRemoveManyFromDisk(std::unordered_set<RouterID> remove) const
  {
    if (m_Root.empty())
      return;
    // build file list
    std::set<fs::path> files;
    for (auto id : remove)
    {
      files.emplace(GetPathForPubkey(std::move(id)));
    }
    // remove them from the disk via the diskio thread
    disk([files]() {
      for (auto fpath : files)
        fs::remove(fpath);
    });
  }

  llarp::RouterContact
  NodeDB::FindClosestTo(llarp::dht::Key_t location) const
  {
    util::NullLock lock{m_Access};
    llarp::RouterContact rc;
    const llarp::dht::XorMetric compare(location);
    VisitAll([&rc, compare](const auto& otherRC) {
      if (rc.pubkey.IsZero())
      {
        rc = otherRC;
        return;
      }
      if (compare(
              llarp::dht::Key_t{otherRC.pubkey.as_array()},
              llarp::dht::Key_t{rc.pubkey.as_array()}))
        rc = otherRC;
    });
    return rc;
  }

  std::vector<RouterContact>
  NodeDB::FindManyClosestTo(llarp::dht::Key_t location, uint32_t numRouters) const
  {
    util::NullLock lock{m_Access};
    std::vector<const RouterContact*> all;

    const auto& entries = m_Entries;

    all.reserve(entries.size());
    for (auto& entry : entries)
    {
      all.push_back(&entry.second.rc);
    }

    auto it_mid = numRouters < all.size() ? all.begin() + numRouters : all.end();
    std::partial_sort(
        all.begin(), it_mid, all.end(), [compare = dht::XorMetric{location}](auto* a, auto* b) {
          return compare(*a, *b);
        });

    std::vector<RouterContact> closest;
    closest.reserve(numRouters);
    for (auto it = all.begin(); it != it_mid; ++it)
      closest.push_back(**it);

    return closest;
  }
}  // namespace llarp
