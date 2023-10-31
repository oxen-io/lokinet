#include "profiling.hpp"

#include "util/file.hpp"

#include <oxenc/bt_producer.h>
#include <oxenc/bt_serialize.h>

using oxenc::bt_dict_consumer;
using oxenc::bt_dict_producer;

namespace llarp
{
  RouterProfile::RouterProfile(bt_dict_consumer dict)
  {
    BDecode(std::move(dict));
  }

  void
  RouterProfile::BEncode(bt_dict_producer& dict) const
  {
    dict.append("g", connectGoodCount);
    dict.append("p", pathSuccessCount);
    dict.append("q", pathTimeoutCount);
    dict.append("s", pathFailCount);
    dict.append("t", connectTimeoutCount);
    dict.append("u", lastUpdated.count());
    dict.append("v", version);
  }

  void
  RouterProfile::BDecode(bt_dict_consumer dict)
  {
    if (dict.skip_until("g"))
      connectGoodCount = dict.consume_integer<uint64_t>();
    if (dict.skip_until("p"))
      pathSuccessCount = dict.consume_integer<uint64_t>();
    if (dict.skip_until("q"))
      pathTimeoutCount = dict.consume_integer<uint64_t>();
    if (dict.skip_until("s"))
      pathFailCount = dict.consume_integer<uint64_t>();
    if (dict.skip_until("t"))
      connectTimeoutCount = dict.consume_integer<uint64_t>();
    if (dict.skip_until("u"))
      lastUpdated = llarp_time_t{dict.consume_integer<uint64_t>()};
    if (dict.skip_until("v"))
      version = dict.consume_integer<uint64_t>();
  }

  void
  RouterProfile::Decay()
  {
    connectGoodCount /= 2;
    connectTimeoutCount /= 2;
    pathSuccessCount /= 2;
    pathFailCount /= 2;
    pathTimeoutCount /= 2;
    lastDecay = llarp::time_now_ms();
  }

  void
  RouterProfile::Tick()
  {
    static constexpr auto updateInterval = 30s;
    const auto now = llarp::time_now_ms();
    if (lastDecay < now && now - lastDecay > updateInterval)
      Decay();
  }

  bool
  RouterProfile::IsGood(uint64_t chances) const
  {
    if (connectTimeoutCount > chances)
      return connectTimeoutCount < connectGoodCount && (pathSuccessCount * chances) > pathFailCount;
    return (pathSuccessCount * chances) > pathFailCount;
  }

  static bool constexpr checkIsGood(uint64_t fails, uint64_t success, uint64_t chances)
  {
    if (fails > 0 && (fails + success) >= chances)
      return (success / fails) > 1;
    if (success == 0)
      return fails < chances;
    return true;
  }

  bool
  RouterProfile::IsGoodForConnect(uint64_t chances) const
  {
    return checkIsGood(connectTimeoutCount, connectGoodCount, chances);
  }

  bool
  RouterProfile::IsGoodForPath(uint64_t chances) const
  {
    if (pathTimeoutCount > chances)
      return false;
    return checkIsGood(pathFailCount, pathSuccessCount, chances);
  }

  Profiling::Profiling() : m_DisableProfiling(false)
  {}

  void
  Profiling::Disable()
  {
    m_DisableProfiling.store(true);
  }

  void
  Profiling::Enable()
  {
    m_DisableProfiling.store(false);
  }

  bool
  Profiling::IsBadForConnect(const RouterID& r, uint64_t chances)
  {
    if (m_DisableProfiling.load())
      return false;
    util::Lock lock{m_ProfilesMutex};
    auto itr = m_Profiles.find(r);
    if (itr == m_Profiles.end())
      return false;
    return not itr->second.IsGoodForConnect(chances);
  }

  bool
  Profiling::IsBadForPath(const RouterID& r, uint64_t chances)
  {
    if (m_DisableProfiling.load())
      return false;
    util::Lock lock{m_ProfilesMutex};
    auto itr = m_Profiles.find(r);
    if (itr == m_Profiles.end())
      return false;
    return not itr->second.IsGoodForPath(chances);
  }

  bool
  Profiling::IsBad(const RouterID& r, uint64_t chances)
  {
    if (m_DisableProfiling.load())
      return false;
    util::Lock lock{m_ProfilesMutex};
    auto itr = m_Profiles.find(r);
    if (itr == m_Profiles.end())
      return false;
    return not itr->second.IsGood(chances);
  }

  void
  Profiling::Tick()
  {
    util::Lock lock(m_ProfilesMutex);
    for (auto& [rid, profile] : m_Profiles)
      profile.Tick();
  }

  void
  Profiling::MarkConnectTimeout(const RouterID& r)
  {
    util::Lock lock{m_ProfilesMutex};
    auto& profile = m_Profiles[r];
    profile.connectTimeoutCount += 1;
    profile.lastUpdated = llarp::time_now_ms();
  }

  void
  Profiling::MarkConnectSuccess(const RouterID& r)
  {
    util::Lock lock{m_ProfilesMutex};
    auto& profile = m_Profiles[r];
    profile.connectGoodCount += 1;
    profile.lastUpdated = llarp::time_now_ms();
  }

  void
  Profiling::ClearProfile(const RouterID& r)
  {
    util::Lock lock{m_ProfilesMutex};
    m_Profiles.erase(r);
  }

  void
  Profiling::MarkHopFail(const RouterID& r)
  {
    util::Lock lock{m_ProfilesMutex};
    auto& profile = m_Profiles[r];
    profile.pathFailCount += 1;
    profile.lastUpdated = llarp::time_now_ms();
  }

  void
  Profiling::MarkPathFail(path::Path* p)
  {
    util::Lock lock{m_ProfilesMutex};
    bool first = true;
    for (const auto& hop : p->hops)
    {
      // don't mark first hop as failure because we are connected to it directly
      if (first)
        first = false;
      else
      {
        auto& profile = m_Profiles[hop.rc.router_id()];
        profile.pathFailCount += 1;
        profile.lastUpdated = llarp::time_now_ms();
      }
    }
  }

  void
  Profiling::MarkPathTimeout(path::Path* p)
  {
    util::Lock lock{m_ProfilesMutex};
    for (const auto& hop : p->hops)
    {
      auto& profile = m_Profiles[hop.rc.router_id()];
      profile.pathTimeoutCount += 1;
      profile.lastUpdated = llarp::time_now_ms();
    }
  }

  void
  Profiling::MarkPathSuccess(path::Path* p)
  {
    util::Lock lock{m_ProfilesMutex};
    const auto sz = p->hops.size();
    for (const auto& hop : p->hops)
    {
      auto& profile = m_Profiles[hop.rc.router_id()];
      // redeem previous fails by halfing the fail count and setting timeout to zero
      profile.pathFailCount /= 2;
      profile.pathTimeoutCount = 0;
      // mark success at hop
      profile.pathSuccessCount += sz;
      profile.lastUpdated = llarp::time_now_ms();
    }
  }

  bool
  Profiling::Save(const fs::path fpath)
  {
    std::string buf;
    {
      util::Lock lock{m_ProfilesMutex};
      buf.resize((m_Profiles.size() * (RouterProfile::MaxSize + 32 + 8)) + 8);
      bt_dict_producer d{buf.data(), buf.size()};
      try
      {
        BEncode(d);
      }
      catch (const std::exception& e)
      {
        log::warning(logcat, "Failed to encode profiling data: {}", e.what());
        return false;
      }
      buf.resize(d.end() - buf.data());
    }

    try
    {
      util::dump_file(fpath, buf);
    }
    catch (const std::exception& e)
    {
      log::warning(logcat, "Failed to save profiling data to {}: {}", fpath, e.what());
      return false;
    }

    m_LastSave = llarp::time_now_ms();
    return true;
  }

  void
  Profiling::BEncode(bt_dict_producer& dict) const
  {
    for (const auto& [r_id, profile] : m_Profiles)
      profile.BEncode(dict.append_dict(r_id.ToView()));
  }

  void
  Profiling::BDecode(bt_dict_consumer dict)
  {
    m_Profiles.clear();
    while (dict)
    {
      auto [rid, subdict] = dict.next_dict_consumer();
      if (rid.size() != RouterID::SIZE)
        throw std::invalid_argument{"invalid RouterID"};
      m_Profiles.emplace(reinterpret_cast<const byte_t*>(rid.data()), subdict);
    }
  }

  bool
  Profiling::Load(const fs::path fname)
  {
    try
    {
      std::string data = util::file_to_string(fname);
      util::Lock lock{m_ProfilesMutex};
      BDecode(bt_dict_consumer{data});
    }
    catch (const std::exception& e)
    {
      log::warning(logcat, "failed to load router profiles from {}: {}", fname, e.what());
      return false;
    }
    m_LastSave = llarp::time_now_ms();
    return true;
  }

  bool
  Profiling::ShouldSave(llarp_time_t now) const
  {
    auto dlt = now - m_LastSave;
    return dlt > 1min;
  }
}  // namespace llarp
