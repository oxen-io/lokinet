#include "bootstrap.hpp"

#include "util/bencode.hpp"
#include "util/logging.hpp"
#include "util/logging/buffer.hpp"

namespace llarp
{
  bool
  BootstrapList::bt_decode(std::string_view buf)
  {
    try
    {
      oxenc::bt_list_consumer btlc{buf};

      while (not btlc.is_finished())
        emplace(btlc.consume_dict_consumer());
    }
    catch (...)
    {
      log::warning(logcat, "Unable to decode bootstrap RemoteRC");
      return false;
    }

    return true;
  }

  bool
  BootstrapList::contains(const RouterID& rid)
  {
    for (const auto& it : *this)
    {
      if (it.router_id() == rid)
        return true;
    }

    return false;
  }

  bool
  BootstrapList::contains(const RemoteRC& rc)
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
  BootstrapList::read_from_file(const fs::path& fpath)
  {
    bool isListFile = false;

    if (std::ifstream inf(fpath.c_str(), std::ios::binary); inf.is_open())
    {
      const char ch = inf.get();
      isListFile = ch == 'l';
    }

    if (isListFile)
    {
      auto content = util::file_to_string(fpath);

      if (not bt_decode(content))
      {
        throw std::runtime_error{fmt::format("failed to read bootstrap list file '{}'", fpath)};
      }
    }
    else
    {
      RemoteRC rc;

      if (not rc.read(fpath))
      {
        throw std::runtime_error{
            fmt::format("failed to decode bootstrap RC, file='{}', rc={}", fpath, rc.to_string())};
      }
      insert(rc);
    }
  }
}  // namespace llarp
