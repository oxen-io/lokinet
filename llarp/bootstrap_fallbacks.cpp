#include <initializer_list>
#include "llarp/bootstrap.hpp"

namespace llarp
{
  using namespace std::literals;

  std::unordered_map<std::string, BootstrapList>
  load_bootstrap_fallbacks()
  {
    std::unordered_map<std::string, BootstrapList> fallbacks;
    using init_list = std::initializer_list<std::pair<std::string, std::string_view>>;
    // clang-format off
    for (const auto& [network, bootstrap] : init_list{
      //
    })
    // clang-format on
    {
      llarp_buffer_t buf{bootstrap.data(), bootstrap.size()};
      auto& bsl = fallbacks[network];
      bsl.BDecode(&buf);
    }
    return fallbacks;
  }
}  // namespace llarp
