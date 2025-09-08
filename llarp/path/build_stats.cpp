#include "build_stats.hpp"

#include <llarp/util/formattable.hpp>
#include <llarp/util/logging.hpp>

#include <nlohmann/json.hpp>

namespace llarp::path
{

    static auto logcat = log::Cat("path");

    nlohmann::json BuildStats::ExtractStatus() const
    {
        return nlohmann::json{
            {"success", success}, {"attempts", attempts}, {"timeouts", timeouts}, {"fails", build_fails}};
    }

    void BuildStats::update(std::chrono::milliseconds now)
    {
        if (attempts > 50 && attempts >= (success * 4) && now - last_warn_time > 5s)
        {
            log::warning(logcat, "Low path build success: {}", *this);
            last_warn_time = now;
        }
    }

    std::string BuildStats::to_string() const
    {
        return "path Stats:[ success:{} | attempts:{} | timeouts:{} | fails:{} ]"_format(
            success, attempts, timeouts, build_fails);
    }

}  // namespace llarp::path
