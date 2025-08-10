#pragma once

#include <llarp/util/time.hpp>

#include <chrono>
#include <cstddef>

namespace llarp::path
{
    /// pad messages to the nearest this many bytes
    inline constexpr std::size_t PAD_SIZE{128};

    /// Number of encryption "frames" inside path builds.  This implicitly defines the maximum
    /// length of a path: shorter path builds still put data in all frames, but frame data beyond
    /// the last hop are random unused data (so that the length of the path build message does not
    /// reveal anything about the total number of hops for the path, and so that the final target
    /// cannot tell how long the path was).
    inline constexpr int BUILD_LENGTH = 8;

    /// Length of each frame of a path build.
    inline constexpr size_t BUILD_FRAME_SIZE = 160;

    inline constexpr auto MAX_LIFETIME = 20min;

    /// How many locations a client contact gets published to.  The contact gets published to the
    /// "closest" [this number] relays, using a metric based on the CC and relay IDs, for short term
    /// redundancy for relays become unreachable or inactive via the Oxen chain.
    ///
    /// (Note that this value cannot be changed without upgrading relays and clients).
    inline constexpr int CC_PUBLISH_LOCATIONS = 4;

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
