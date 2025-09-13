#pragma once

#include <llarp/contact/router_id.hpp>
#include <llarp/util/time.hpp>

#include <chrono>
#include <queue>
#include <random>
#include <unordered_set>
#include <vector>

namespace llarp
{
    class Router;
}
namespace oxen::quic
{
    class Ticker;
}

namespace llarp::consensus
{
    namespace detail
    {
        using clock_t = std::chrono::steady_clock;
        using time_point_t = std::chrono::time_point<clock_t>;

        // Returns std::greater on the std::get<N>(v)th element value.
        template <typename T, size_t N>
        struct nth_greater
        {
            constexpr bool operator()(const T& lhs, const T& rhs) const
            {
                return std::greater<std::tuple_element_t<N, T>>{}(std::get<N>(lhs), std::get<N>(rhs));
            }
        };

        struct incoming_test_state
        {
            time_point_t last_test{};
            time_point_t last_whine{};
            bool was_failing = false;
        };

    }  // namespace detail
    using time_point_t = detail::time_point_t;
    using clock_t = detail::clock_t;

    using fseconds = std::chrono::duration<float, std::chrono::seconds::period>;
    using fminutes = std::chrono::duration<float, std::chrono::minutes::period>;

    class reachability_testing
    {
      public:
        // How often we tick the timer to perform one new random test and check whether we need to
        // do any re-tests.  The determines the overall testing rate (i.e. with 2 random tests per
        // second, it would take 16.6 minutes to cycle through a full list of 2000 service nodes).
        static constexpr auto TEST_INTERVAL = 333'333us;

        // The maximum number of nodes that we will re-test at once (i.e. per
        // TESTING_TIMING_INTERVAL); mainly intended to throttle ourselves if, for instance, our own
        // connectivity loss makes us accumulate tons of nodes to test all at once.
        static constexpr int MAX_RETESTS_PER_TICK = 3;

        // The backoff after each consecutive test failure before we re-test.  Specifically we
        // schedule the next re-test after a failure to occur in
        //
        //     (RETEST_BACKOFF*consecutive_failures) + RETEST_NOISE(rng)
        //
        // seconds (truncated to [0, RETEST_MAX]), where the randomness is to avoid clustering of
        // retests.
        static constexpr auto RETEST_BACKOFF = 10s;
        std::normal_distribution<float> RETEST_NOISE{0, 3};
        static constexpr auto RETEST_MAX = 2min;

        // Returns a testing interval (as per above) for a node that has failed the last
        // `n_failures` consecutive tests.
        std::chrono::microseconds retest_interval(int n_failures);

        // How long we allow the test request to take.  This time is the maximum allowed time for
        // both establishing the connection and making the request once established.
        static constexpr auto TEST_REQUEST_TIMEOUT = 4s;

        // Maximum time without an incoming testing ping before we start whining about it (if we are
        // registered), as that likely means that we are currently unreachable.
        static constexpr auto MAX_TIME_WITHOUT_PING = 2min;

        // Rate-limit for how often we whine in the logs about looking unreachable because we
        // haven't received any recent pings.
        static constexpr auto WHINING_INTERVAL = 5min;

      private:
        Router& router;

        std::shared_ptr<oxen::quic::Ticker> ticker;
        std::shared_ptr<oxen::quic::Ticker> whine_ticker;

        // Queue of pubkeys of service nodes to test; we pop off the back of this until the queue
        // empties then we refill it with a shuffled list of all pubkeys then pull off of it until
        // it is empty again, etc.
        std::vector<RouterID> testing_queue;

        // When we started, so that we know not to hold off on whining about no pings for a while.
        const time_point_t startup = clock_t::now();

        // Pubkeys, next test times, and sequential failure counts of service nodes that are
        // currently in "failed" status.
        using FailingPK = std::tuple<RouterID, time_point_t, int>;
        std::priority_queue<FailingPK, std::vector<FailingPK>, detail::nth_greater<FailingPK, 1>> failing_queue;
        std::unordered_set<RouterID> failing;

        // Track the last time *this node* was tested by other network nodes; used to detect and
        // warn about possible network issues.
        detail::incoming_test_state last;

      public:
        explicit reachability_testing(Router& r);

        // Called by router when it is starting/stopping to start/stop our ticker.
        void start();
        void stop();

        // Runs a tick iteration.
        void tick();

        // Returns the next random node to test from the random testing queue, skipping any nodes
        // that are currently in the failed nodes queue.  If the random queue is empty, this will
        // replenish it with a shuffled list of all known registered relays IDs.  If the we still
        // can't find any relay after replenishing, this returns nullopt.
        //
        // `_requeue` is for internal use only and should not be given explicitly.
        std::optional<RouterID> next_random(bool _requeue = true);

        // Removes and returns up to MAX_RETESTS_PER_TICK nodes that are due to be tested (i.e.
        // next-testing-time <= now).  Returns [snrecord, #previous-failures] for each.
        std::vector<std::pair<RouterID, int>> get_failing(const time_point_t& now = clock_t::now());

        // Adds a bad node pubkey to the failing list, to be re-tested soon (with a backoff
        // depending on `failures`; see TESTING_BACKOFF).  `previous_failures` should be the number
        // of previous failures *before* this one, i.e. 0 for a random general test; or the failure
        // count returned by `get_failing` for repeated failures.
        void add_failing_node(const RouterID& pk, int previous_failures = 0);

        /// removes the public key from the failing set
        void remove_node_from_failing(const RouterID& pk);

        // Called when this router receives an incoming ping test request
        void incoming_ping(const time_point_t& now = clock_t::now());

        // Check whether we received incoming pings recently
        void check_incoming_tests(const time_point_t& now = clock_t::now());
    };

}  // namespace llarp::consensus
