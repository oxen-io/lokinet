#include <catch2/catch.hpp>
#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <string_view>

#include <router_contact.hpp>
#include <iwp/iwp.hpp>
#include <util/meta/memfn.hpp>
#include <messages/link_message_parser.hpp>
#include <messages/discard.hpp>
#include <util/time.hpp>

#include <net/net_if.hpp>
#include "ev/ev.hpp"

#undef LOG_TAG
#define LOG_TAG __FILE__

namespace iwp = llarp::iwp;
namespace util = llarp::util;

/// make an iwp link
template <bool inbound, typename... Args>
static llarp::LinkLayer_ptr
make_link(Args&&... args)
{
  if (inbound)
    return iwp::NewInboundLink(std::forward<Args>(args)...);
  return iwp::NewOutboundLink(std::forward<Args>(args)...);
}

/// a single iwp link with associated keys and members to make unit tests work
struct IWPLinkContext
{
  llarp::RouterContact rc;
  llarp::IpAddress localAddr;
  llarp::LinkLayer_ptr link;
  std::shared_ptr<llarp::KeyManager> keyManager;
  llarp::LinkMessageParser m_Parser;
  llarp::EventLoop_ptr m_Loop;
  /// is the test done on this context ?
  bool gucci = false;

  IWPLinkContext(std::string_view addr, llarp::EventLoop_ptr loop)
      : localAddr{std::move(addr)}
      , keyManager{std::make_shared<llarp::KeyManager>()}
      , m_Parser{nullptr}
      , m_Loop{std::move(loop)}
  {
    // generate keys
    llarp::CryptoManager::instance()->identity_keygen(keyManager->identityKey);
    llarp::CryptoManager::instance()->encryption_keygen(keyManager->encryptionKey);
    llarp::CryptoManager::instance()->encryption_keygen(keyManager->transportKey);

    // set keys in rc
    rc.pubkey = keyManager->identityKey.toPublic();
    rc.enckey = keyManager->encryptionKey.toPublic();
  }

  template <typename Func_t>
  void
  Call(Func_t work)
  {
    m_Loop->call_soon(std::move(work));
  }

  bool
  HandleMessage(llarp::ILinkSession* from, const llarp_buffer_t& buf)
  {
    return m_Parser.ProcessFrom(from, buf);
  }

  /// initialize link
  template <bool inbound>
  void
  InitLink(std::function<void(llarp::ILinkSession*)> established)
  {
    link = make_link<inbound>(
        keyManager,
        m_Loop,
        // getrc
        [&]() -> const llarp::RouterContact& { return rc; },
        // link message handler
        util::memFn(&IWPLinkContext::HandleMessage, this),
        // sign buffer
        [&](llarp::Signature& sig, const llarp_buffer_t& buf) {
          REQUIRE(llarp::CryptoManager::instance()->sign(sig, keyManager->identityKey, buf));
          return true;
        },
        // before connect
        nullptr,
        // established handler
        [established](llarp::ILinkSession* s, bool linkIsInbound) {
          REQUIRE(s != nullptr);
          REQUIRE(inbound == linkIsInbound);
          established(s);
          return true;
        },
        // renegotiate handler
        [](llarp::RouterContact newrc, llarp::RouterContact oldrc) {
          REQUIRE(newrc.pubkey == oldrc.pubkey);
          return true;
        },
        // timeout handler
        [&](llarp::ILinkSession*) {
          m_Loop->stop();
          FAIL("session timeout");
        },
        // session closed handler
        [](llarp::RouterID) {},
        // pump done handler
        []() {},
        // do work function
        [l = m_Loop](llarp::Work_t work) { l->call_soon(work); });
    REQUIRE(link->Configure(
        m_Loop, llarp::net::LoopbackInterfaceName(), AF_INET, *localAddr.getPort()));

    if (inbound)
    {
      // only add address info on the recipient's rc
      rc.addrs.emplace_back();
      REQUIRE(link->GetOurAddressInfo(rc.addrs.back()));
    }
    // sign rc
    REQUIRE(rc.Sign(keyManager->identityKey));
    REQUIRE(keyManager != nullptr);
  }
};

using Context_ptr = std::shared_ptr<IWPLinkContext>;

/// run an iwp unit test after setup
/// call take 2 parameters, test and a timeout
///
/// test is a callable that takes 5 arguments:
/// 0) std::function<EventLoop_ptr(void)> that starts the iwp links and gives an event loop to call with
/// 1) std::function<void(void)> that ends the unit test if we are done
/// 2) std::function<void(void)> that ends the unit test right now as a success
/// 3) client iwp link context (shared_ptr)
/// 4) relay iwp link context (shared_ptr)
///
/// timeout is a std::chrono::duration that tells the driver how long to run the unit test for
/// before it should assume failure of unit test
template <typename Func_t, typename Duration_t = std::chrono::milliseconds>
void
RunIWPTest(Func_t test, Duration_t timeout = 10s)
{
  // shut up logs
  llarp::LogSilencer shutup;
  // set up event loop
  auto loop = llarp::EventLoop::create();

  llarp::LogContext::Instance().Initialize(
      llarp::eLogDebug, llarp::LogType::File, "stdout", "unit test", [loop](auto work) {
        loop->call_soon(work);
      });

  // turn off bogon blocking
  auto oldBlockBogons = llarp::RouterContact::BlockBogons;
  llarp::RouterContact::BlockBogons = false;

  // set up cryptography
  llarp::sodium::CryptoLibSodium crypto{};
  llarp::CryptoManager manager{&crypto};

  // set up client
  auto initiator = std::make_shared<IWPLinkContext>("127.0.0.1:3001", loop);
  // set up server
  auto recipient = std::make_shared<IWPLinkContext>("127.0.0.1:3002", loop);

  // function for ending unit test on success
  auto endIfDone = [initiator, recipient, loop]() {
    if (initiator->gucci and recipient->gucci)
    {
      loop->stop();
    }
  };
  // function to start test and give loop to unit test
  auto start = [initiator, recipient, loop]() {
    REQUIRE(initiator->link->Start());
    REQUIRE(recipient->link->Start());
    return loop;
  };

  // function to end test immediately
  auto endTest = [loop] { loop->stop(); };

  loop->call_later(timeout, [] { FAIL("test timeout"); });
  test(start, endIfDone, endTest, initiator, recipient);
  loop->run();
  llarp::RouterContact::BlockBogons = oldBlockBogons;
}

/// ensure clients can connect to relays
TEST_CASE("IWP handshake", "[iwp]")
{
  RunIWPTest([](std::function<llarp::EventLoop_ptr(void)> start,
                std::function<void(void)> endIfDone,
                [[maybe_unused]] std::function<void(void)> endTestNow,
                Context_ptr alice,
                Context_ptr bob) {
    // set up initiator
    alice->InitLink<false>([=](auto remote) {
      REQUIRE(remote->GetRemoteRC() == bob->rc);
      alice->gucci = true;
      endIfDone();
    });
    // set up recipient
    bob->InitLink<true>([=](auto remote) {
      REQUIRE(remote->GetRemoteRC() == alice->rc);
      bob->gucci = true;
      endIfDone();
    });
    // start unit test
    auto loop = start();
    // try establishing a session
    loop->call([link = alice->link, rc = bob->rc]() { REQUIRE(link->TryEstablishTo(rc)); });
  });
}

/// ensure relays cannot connect to clients
TEST_CASE("IWP handshake reverse", "[iwp]")
{
  RunIWPTest([](std::function<llarp::EventLoop_ptr(void)> start,
                [[maybe_unused]] std::function<void(void)> endIfDone,
                std::function<void(void)> endTestNow,
                Context_ptr alice,
                Context_ptr bob) {
    alice->InitLink<false>([](auto) {});
    bob->InitLink<true>([](auto) {});
    // start unit test
    auto loop = start();
    // try establishing a session in the wrong direction
    loop->call([link = bob->link, rc = alice->rc, endTestNow] {
      REQUIRE(not link->TryEstablishTo(rc));
      endTestNow();
    });
  });
}

/// ensure iwp can send messages between sessions
TEST_CASE("IWP send messages", "[iwp]")
{
  int aliceNumSent = 0;
  int bobNumSent = 0;
  RunIWPTest([&aliceNumSent, &bobNumSent](std::function<llarp::EventLoop_ptr(void)> start,
                std::function<void(void)> endIfDone,
                std::function<void(void)> endTestNow,
                Context_ptr alice,
                Context_ptr bob) {
    constexpr int numSend = 64;
    // when alice makes a session to bob send `aliceNumSend` messages to him
    alice->InitLink<false>([endIfDone, alice, &aliceNumSent](auto session) {
      for (auto index = 0; index < numSend; index++)
      {
        alice->Call([session, endIfDone, alice, &aliceNumSent]() {
          // generate a discard message that is 512 bytes long
          llarp::DiscardMessage msg;
          std::vector<byte_t> msgBuff(512);
          llarp_buffer_t buf(msgBuff);
          // add random padding
          llarp::CryptoManager::instance()->randomize(buf);
          // encode the discard message
          msg.BEncode(&buf);
          // send the message
          session->SendMessageBuffer(msgBuff, [endIfDone, alice, &aliceNumSent](auto status) {
            if (status == llarp::ILinkSession::DeliveryStatus::eDeliverySuccess)
            {
              // on successful transmit increment the number we sent
              aliceNumSent++;
            }
            // if we sent all the messages sucessfully we end the unit test
            alice->gucci = aliceNumSent == numSend;
            endIfDone();
          });
        });
      }
    });
    bob->InitLink<true>([endIfDone, bob, &bobNumSent](auto session) {
      for (auto index = 0; index < numSend; index++)
      {
        bob->Call([session, endIfDone, bob, &bobNumSent]() {
          // generate a discard message that is 512 bytes long
          llarp::DiscardMessage msg;
          std::vector<byte_t> msgBuff(512);
          llarp_buffer_t buf(msgBuff);
          // add random padding
          llarp::CryptoManager::instance()->randomize(buf);
          // encode the discard message
          msg.BEncode(&buf);
          // send the message
          session->SendMessageBuffer(msgBuff, [endIfDone, bob, &bobNumSent](auto status) {
            if (status == llarp::ILinkSession::DeliveryStatus::eDeliverySuccess)
            {
              // on successful transmit increment the number we sent
              bobNumSent++;
            }
            // if we sent all the messages sucessfully we end the unit test
            bob->gucci = bobNumSent == numSend;
            endIfDone();
          });
        });
      }
    });
    // start unit test
    auto loop = start();
    // try establishing a session from alice to bob
    loop->call([link = alice->link, rc = bob->rc, endTestNow]() {
      REQUIRE(link->TryEstablishTo(rc));
    });
  });
}
