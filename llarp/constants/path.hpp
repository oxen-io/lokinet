#pragma once

#include <llarp/util/time.hpp>

#include <chrono>
#include <cstddef>

namespace llarp::path
{
    /// maximum path length
    inline constexpr std::size_t MAX_LEN{8};

    /// default path length
    inline constexpr std::size_t DEFAULT_LEN{4};

    /// pad messages to the nearest this many bytes
    inline constexpr std::size_t PAD_SIZE{128};

    // default number of paths per PathHandler
    inline constexpr size_t DEFAULT_PATHS_HELD{4};

    /// TESTNET: default path lifetime in ms;
    inline constexpr std::chrono::milliseconds DEFAULT_LIFETIME{20min};

    /// interval at which we try to build new paths for intros
    inline constexpr std::chrono::milliseconds PATH_ROTATION_INTERVAL{DEFAULT_LIFETIME / DEFAULT_PATHS_HELD};

    /// after this many ms a path build times out
    inline constexpr auto BUILD_TIMEOUT{10s};

    inline constexpr auto MIN_PATH_BUILD_INTERVAL{500ms};

    inline constexpr auto PATH_BUILD_RATE{100ms};

    /// measure latency every this interval ms
    inline constexpr std::chrono::milliseconds LATENCY_INTERVAL{20s};

    /// if a path is inactive for this amount of time it's dead
    inline constexpr std::chrono::milliseconds ALIVE_TIMEOUT{LATENCY_INTERVAL * 3 / 2};

    /// how big transit hop traffic queues are
    inline constexpr std::size_t TRANSIT_HOP_QUEUE_SIZE{256};

}  // namespace llarp::path
