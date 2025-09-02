#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <cstdint>

namespace llarp::path
{

    using namespace std::literals;

    /// Stats about all our path builds
    struct BuildStats
    {
        uint64_t attempts{0};
        uint64_t success{0};
        uint64_t build_fails{0};  // path build failures
        uint64_t path_fails{0};   // path failures post-build
        uint64_t timeouts{0};

        std::chrono::milliseconds last_warn_time{0ms};

        nlohmann::json ExtractStatus() const;

        void update(std::chrono::milliseconds now);

        std::string to_string() const;
        static constexpr bool to_string_formattable = true;
    };

}  // namespace llarp::path
