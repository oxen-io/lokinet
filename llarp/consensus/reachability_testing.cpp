#include "reachability_testing.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/link/endpoint.hpp>
#include <llarp/nodedb.hpp>
#include <llarp/router/router.hpp>
#include <llarp/rpc/oxend_rpc.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/random.hpp>

#include <chrono>

using std::chrono::steady_clock;

namespace llarp::consensus
{
    static auto logcat = log::Cat("testing");

    reachability_testing::reachability_testing(Router& r) : router{r} {}

    void reachability_testing::start()
    {
        if (router.config().lokid.disable_testing)
            log::warning(logcat, "Reachability testing DISABLED in config");
        else
        {
            log::debug(logcat, "Starting reachability testing tickers");
            ticker = router.loop.call_every(TEST_INTERVAL, [this] { tick(); });
            whine_ticker = router.loop.call_every(30s, [this] { check_incoming_tests(); });
        }
    }

    void reachability_testing::stop()
    {
        ticker.reset();
        whine_ticker.reset();
    }

    void reachability_testing::tick()
    {
        // Reachability testing is currently quite simple: when we want to test a node we
        // connect to it using a *client ALPN* (so that the connection is not treated as a relay
        // connection by either end), call the "ping" endpoint over it, wait for the pong, then
        // disconnect.

        // Don't run tests if we are not running or we are stopping, or aren't currently
        // registered
        if (not router.is_running() or not router.appears_registered())
            return;

        auto tests = get_failing();

        if (auto maybe = next_random())
            tests.emplace_back(*maybe, 0);

        auto now = llarp::time_now_ms();

        auto& node_db = router.node_db();

        log::debug(logcat, "{} service nodes for connectivity testing", tests.size());

        for (const auto& [rid, prev_fails] : tests)
        {
            if (not node_db.is_registered(rid))
            {
                log::debug(logcat, "{} is no longer a registered service node; dropping from test list", rid);
                remove_node_from_failing(rid);
                continue;
            }

            log::debug(
                logcat, "Establishing test connection to {} for reachability testing", rid.to_network_address(true));

            auto* rc = node_db.get_rc(rid);
            if (!rc || rc->is_outdated(now))
            {
                log::debug(logcat, "Test failed for {}: have no recent RC", rid.to_network_address(true));
                add_failing_node(rid, prev_fails);
                continue;
            }

            auto [conn, btstr] = router.link_endpoint().testing_client_connect(*rc);
            btstr->command(
                "ping",
                "",
                TEST_REQUEST_TIMEOUT,
                [this, weak_conn = std::weak_ptr{conn}, rid, prev_fails](quic::message m) mutable {
                    auto conn = weak_conn.lock();
                    if (conn)
                        conn->close_connection();
                    router.loop.call_soon([this, rid, prev_fails, m = std::move(m)] {
                        if (m)
                        {
                            if (prev_fails)
                                log::info(
                                    logcat,
                                    "Successful SN reachability test to {} (after {} previous failures)",
                                    rid.to_network_address(true),
                                    prev_fails);
                            else
                                log::info(
                                    logcat, "Successful SN reachability test to {}", rid.to_network_address(true));
                            remove_node_from_failing(rid);
                        }
                        else
                        {
                            log::info(
                                logcat,
                                "Testing of {} failed: {}",
                                rid.to_network_address(true),
                                m.timed_out ? "request timed out" : m.body());
                            add_failing_node(rid, prev_fails);
                        }
                        router.oxend()->inform_connection(rid, (bool)m);
                    });
                });
        }
    }

    void reachability_testing::check_incoming_tests(const time_point_t& now)
    {
        if (not router.appears_registered())
        {
            if (not last.was_unregistered and last.was_failing)
                log::info(log_global, "Disabling incoming ping warnings: service node is no longer registered");
            else
                log::debug(logcat, "Not checking incoming tests: not a registered relay");
            last.was_unregistered = true;
            last.was_failing = false;
            return;
        }

        const auto elapsed = now - std::max(startup, last.last_test);

        // If we just became registered then use a longer threshold without pings before we flag it
        // as a problem: it can take some time for nodes to get through their current queues and
        // build a new queue that includes us.
        bool failing = elapsed > (last.was_unregistered ? MAX_TIME_INITIAL : MAX_TIME_WITHOUT_PING);

        bool whine = failing != last.was_failing || (failing && now - last.last_whine > WHINING_INTERVAL);

        last.was_failing = failing;

        if (whine)
        {
            last.last_whine = now;
            if (!failing)
                log::info(log_global, "Received test ping; port is likely reachable!");
            else
            {
                if (last.last_test.time_since_epoch() == 0s)
                    log::warning(log_global, "Have NEVER received test pings!");
                else
                    log::warning(log_global, "Have not received pings in {:.1f} minutes", fminutes{elapsed}.count());

                log::warning(
                    log_global, "Check configured IP and port: Non-reachabibility may result in deregistration!");
            }
        }
    }

    void reachability_testing::incoming_ping(const time_point_t& now)
    {
        last.last_test = now;

        // If we had previous logged about a failure then log about the success immediately (rather
        // than waiting for the next whine tick):
        if (last.was_failing)
            check_incoming_tests();
    }

    std::chrono::microseconds reachability_testing::retest_interval(int n_failures)
    {
        if (n_failures < 1)
            n_failures = 1;
        return std::clamp<std::chrono::microseconds>(
            n_failures * RETEST_BACKOFF
                + duration_cast<std::chrono::microseconds>(fseconds{RETEST_NOISE(llarp::csrng)}),
            0s,
            RETEST_MAX);
    }

    std::optional<RouterID> reachability_testing::next_random(bool _requeue)
    {
        std::optional<RouterID> sn;

        // Pull the next element off the queue, but skip ourself, any that are no longer registered,
        // and any that are currently known to be failing (those are queued for testing separately).
        while (!testing_queue.empty())
        {
            auto& pk = testing_queue.back();

            if (!failing.count(pk))
                sn = pk;

            testing_queue.pop_back();

            if (sn)
                return sn;
        }

        if (!_requeue)
            // If we get here then we already tried rebuilding and still found nothing, so give up.
            return std::nullopt;

        // We exhausted the queue so repopulate it and try again from the top
        testing_queue = router.node_db().get_registered_relays();
        std::shuffle(testing_queue.begin(), testing_queue.end(), llarp::csrng);

        // Recurse with the rebuilt list, but don't let it try rebuilding again if it exhausts the
        // fresh list.
        sn = next_random(/*_requeue=*/false);
        return sn;
    }

    std::vector<std::pair<RouterID, int>> reachability_testing::get_failing(const time_point_t& now)
    {
        // Our failing_queue puts the oldest retest times at the top, so pop them off into our
        // result until the top node should be retested sometime in the future (or we have enough).
        std::vector<std::pair<RouterID, int>> result;
        while (not failing_queue.empty() and result.size() < MAX_RETESTS_PER_TICK)
        {
            auto& [pk, retest_time, failures] = failing_queue.top();
            if (retest_time > now)
                break;
            if (failing.count(pk))
                result.emplace_back(pk, failures);
            failing_queue.pop();
        }
        return result;
    }

    void reachability_testing::add_failing_node(const RouterID& pk, int previous_failures)
    {
        if (previous_failures < 0)
            previous_failures = 0;
        ++previous_failures;
        failing.insert(pk);
        failing_queue.emplace(
            pk, std::chrono::steady_clock::now() + retest_interval(previous_failures), previous_failures);
    }

    void reachability_testing::remove_node_from_failing(const RouterID& pk) { failing.erase(pk); }

}  // namespace llarp::consensus
