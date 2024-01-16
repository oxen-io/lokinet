#include "bootstrap.hpp"

#include "util/bencode.hpp"
#include "util/logging.hpp"
#include "util/logging/buffer.hpp"

namespace llarp
{
  bool
  BootstrapList::bt_decode(std::string_view buf)
  {
    const auto& f = buf.front();

    switch (f)
    {
      case 'l':
        return bt_decode(oxenc::bt_list_consumer{buf});
      case 'd':
        return bt_decode(oxenc::bt_dict_consumer{buf});
      default:
        log::critical(logcat, "Unable to parse bootstrap as bt list or dict!");
        return false;
    }
  }

  bool
  BootstrapList::bt_decode(oxenc::bt_list_consumer btlc)
  {
    try
    {
      while (not btlc.is_finished())
        emplace(btlc.consume_dict_data());
    }
    catch (...)
    {
      log::warning(logcat, "Unable to decode bootstrap RemoteRC");
      return false;
    }

    _curr = begin();
    return true;
  }

  bool
  BootstrapList::bt_decode(oxenc::bt_dict_consumer btdc)
  {
    try
    {
      emplace(btdc);
    }
    catch (const std::exception& e)
    {
      log::warning(logcat, "Unable to decode bootstrap RemoteRC: {}", e.what());
      return false;
    }

    _curr = begin();
    return true;
  }

  bool
  BootstrapList::contains(const RouterID& rid) const
  {
    for (const auto& it : *this)
    {
      if (it.router_id() == rid)
        return true;
    }

    return false;
  }

  bool
  BootstrapList::contains(const RemoteRC& rc) const
  {
    return count(rc);
  }

  std::string_view
  BootstrapList::bt_encode() const
  {
    oxenc::bt_list_producer btlp{};

    for (const auto& it : *this)
      btlp.append(it.view());

    return btlp.view();
  }

  void
  BootstrapList::populate_bootstraps(
      std::vector<fs::path> paths, const fs::path& def, bool load_fallbacks)
  {
    for (const auto& f : paths)
    {
      // TESTNET: TODO: revise fucked config
      log::debug(logcat, "Loading BootstrapRC from file at path:{}", f);
      if (not read_from_file(f))
        throw std::invalid_argument{"User-provided BootstrapRC is invalid!"};
    }

    if (empty())
    {
      log::debug(
          logcat,
          "BootstrapRC list empty; looking for default BootstrapRC from file at path:{}",
          def);
      read_from_file(def);
    }

    for (auto itr = begin(); itr != end(); ++itr)
    {
      if (RouterContact::is_obsolete(*itr))  // can move this into ::read_from_file
      {
        log::critical(logcat, "Deleting obsolete BootstrapRC (rid:{})", itr->router_id());
        itr = erase(itr);
        continue;
      }
    }

    if (empty() and load_fallbacks)
    {
      log::critical(logcat, "BootstrapRC list empty; loading fallbacks...");
      auto fallbacks = llarp::load_bootstrap_fallbacks();

      if (auto itr = fallbacks.find(RouterContact::ACTIVE_NETID); itr != fallbacks.end())
      {
        log::critical(
            logcat, "Loading {} default fallback bootstrap router(s)!", itr->second.size());
        merge(itr->second);
      }

      if (empty())
      {
        log::error(
            logcat,
            "No Bootstrap routers were loaded.  The default Bootstrap file {} does not exist, and "
            "loading fallback Bootstrap RCs failed.",
            def);

        throw std::runtime_error("No Bootstrap nodes available.");
      }
    }

    log::critical(logcat, "We have {} Bootstrap router(s)!", size());
    _curr = begin();
  }

  bool
  BootstrapList::read_from_file(const fs::path& fpath)
  {
    bool result = false;

    if (not fs::exists(fpath))
    {
      log::critical(logcat, "Bootstrap RC file non-existant at path:{}", fpath);
      return result;
    }

    auto content = util::file_to_string(fpath);
    result = bt_decode(content);

    log::critical(
        logcat, "{}uccessfully loaded BootstrapRC file at path:{}", result ? "S" : "Un", fpath);

    _curr = begin();
    return result;
  }
}  // namespace llarp
