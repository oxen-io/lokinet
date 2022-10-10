#include "bootstrap.hpp"
#include "util/bencode.hpp"
#include "util/logging.hpp"
#include "util/logging/buffer.hpp"

namespace llarp
{
  void
  BootstrapList::Clear()
  {
    clear();
  }

  bool
  BootstrapList::BDecode(llarp_buffer_t* buf)
  {
    return bencode_read_list(
        [&](llarp_buffer_t* b, bool more) -> bool {
          if (more)
          {
            RouterContact rc{};
            if (not rc.BDecode(b))
            {
              LogError("invalid rc in bootstrap list: ", llarp::buffer_printer{*b});
              return false;
            }
            emplace(std::move(rc));
          }
          return true;
        },
        buf);
  }

  bool
  BootstrapList::BEncode(llarp_buffer_t* buf) const
  {
    return BEncodeWriteList(begin(), end(), buf);
  }

  void
  BootstrapList::AddFromFile(fs::path fpath)
  {
    bool isListFile = false;
    {
      std::ifstream inf(fpath.c_str(), std::ios::binary);
      if (inf.is_open())
      {
        const char ch = inf.get();
        isListFile = ch == 'l';
      }
    }
    if (isListFile)
    {
      if (not BDecodeReadFile(fpath, *this))
      {
        throw std::runtime_error{fmt::format("failed to read bootstrap list file '{}'", fpath)};
      }
    }
    else
    {
      RouterContact rc;
      if (not rc.Read(fpath))
      {
        throw std::runtime_error{
            fmt::format("failed to decode bootstrap RC, file='{}', rc={}", fpath, rc)};
      }
      this->insert(rc);
    }
  }
}  // namespace llarp
