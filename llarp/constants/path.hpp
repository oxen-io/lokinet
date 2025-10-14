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
    inline constexpr size_t BUILD_FRAME_SIZE = 169;

    /// Max base lifetime of paths.  This is the lifetime of outbound paths, and is the maximum
    /// target lifetime of inbound paths.  Inbound paths also have up some random fuzz added to
    /// this, and so the actual maximum allowed by a relay is be slightly higher than this; see
    /// next two variables.
    inline constexpr std::chrono::seconds MAX_LIFETIME = 20min;

    /// Maximum path expiry randomness: when building paths we add a random value up to this amount
    /// to the path lifetime, and so relays will accept paths of up to MAX_LIFETIME plus this value.
    inline constexpr std::chrono::seconds MAX_LIFETIME_FUZZ = 3min;

    /// The maximum path life accepted by a relay: this is the maximum life plus the maximum amount
    /// of random fuzz.
    inline constexpr std::chrono::seconds MAX_LIFETIME_ACCEPTED = MAX_LIFETIME + MAX_LIFETIME_FUZZ;

    /// The minimum expiry time slots for inbound paths.  See detailed comments in
    /// SessionEndpoint::update_paths().
    inline constexpr auto MAX_LIFETIME_SLOTS = 4;

    static_assert(
        std::chrono::seconds{MAX_LIFETIME} % MAX_LIFETIME_SLOTS == 0s,
        "MAX_LIFETIME_SLOTS must evenly divide MAX_LIFETIME seconds");

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
