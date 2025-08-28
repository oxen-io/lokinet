#pragma once

#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <random>
#include <type_traits>

using namespace std::literals;

namespace llarp
{
    // Libevent uses µs precision
    using loop_time = std::chrono::microseconds;

    /// get time right now as milliseconds, this is monotonic
    std::chrono::milliseconds time_now_ms();

    /// get the uptime of the process
    std::chrono::milliseconds uptime();

    /// convert to milliseconds
    uint64_t to_milliseconds(std::chrono::milliseconds duration);

    nlohmann::json to_json(const std::chrono::milliseconds& t);

    // Returns a string such as "27m13s ago" or "in 1h12m" or "now".  You get precision of minutes
    // (for >=1h), seconds (>=10s), or milliseconds.  The `now_threshold` argument controls how
    // close to current time (default 1s) the time has to be to get the "now" argument.
    std::string short_time_from_now(
        const std::chrono::system_clock::time_point& t, const std::chrono::milliseconds& now_threshold = 1s);

    inline timeval loop_time_to_timeval(loop_time t)
    {
        return timeval{
            .tv_sec = static_cast<decltype(timeval::tv_sec)>(t / 1s),
            .tv_usec = static_cast<decltype(timeval::tv_usec)>((t % 1s) / 1us)};
    }

    std::chrono::nanoseconds get_timestamp();

    template <typename unit_t>
    auto get_timestamp()
    {
        return std::chrono::duration_cast<unit_t>(get_timestamp());
    }

    /** Returns a duration uniformly distributed between `a` and `b`.  E.g.
     *
     *     auto t = llarp::uniform_duration_distribution{5min, 8min}(llarp);
     *
     * yields a duration uniformly distributed in [5min, 8min], with `Time` precision.
     *
     * Time defaults to at least milliseconds (if given less precise types, such as minutes), but
     * will be more precise if constructed with more precise duration types.
     */
    template <typename Time>
    struct uniform_duration_distribution
    {
        using underlying_rep = Time::rep;
        std::conditional_t<
            std::is_floating_point_v<underlying_rep>,
            std::uniform_real_distribution<underlying_rep>,
            std::uniform_int_distribution<underlying_rep>>
            underlying_dist;

        using result_type = Time;

        constexpr uniform_duration_distribution(Time a, Time b) : underlying_dist{a.count(), b.count()} {}

        template <class Generator>
        Time operator()(Generator& g)
        {
            return Time{underlying_dist(g)};
        }
    };

    template <typename TimeA, typename TimeB>
    uniform_duration_distribution(TimeA a, TimeB b)
        -> uniform_duration_distribution<std::common_type_t<TimeA, TimeB, std::chrono::milliseconds>>;

}  // namespace llarp
