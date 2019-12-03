#include <crypto/crypto_libsodium.hpp>
#include <ev/ev.h>
#include <iwp/iwp.hpp>
#include <llarp_test.hpp>
#include <iwp/iwp.hpp>
#include <messages/link_intro.hpp>
#include <messages/discard.hpp>



#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace ::llarp;
using namespace ::testing;

struct LinkLayerTest : public test::LlarpTest< llarp::sodium::CryptoLibSodium >
{
  static constexpr uint16_t AlicePort = 5000;
  static constexpr uint16_t BobPort   = 6000;

  struct Context
  {
    Context()
    {
      CryptoManager::instance()->identity_keygen(signingKey);
      CryptoManager::instance()->encryption_keygen(encryptionKey);
      rc.pubkey = signingKey.toPublic();
      rc.enckey = encryptionKey.toPublic();
    }

    std::shared_ptr<thread::ThreadPool> worker;

    SecretKey signingKey;
    SecretKey encryptionKey;

    RouterContact rc;

    bool madeSession = false;
    bool gotLIM = false;

    bool 
    IsGucci() const
    {
      return gotLIM && madeSession;
    }

    void Setup()
    {
        worker = std::make_shared<thread::ThreadPool>(1, 128, "test-worker");
        worker->start();
    }

    const RouterContact&
    GetRC() const
    {
      return rc;
    }

    RouterID
    GetRouterID() const
    {
      return rc.pubkey;
    }

    /// regenerate rc and rotate onion key
    bool
    Regen()
    {
      CryptoManager::instance()->encryption_keygen(encryptionKey);
      rc.enckey = seckey_topublic(encryptionKey);
      return rc.Sign(signingKey);
    }

    std::shared_ptr< ILinkLayer > link;

    static std::string
    localLoopBack()
    {
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__) || (__sun)
      return "lo0";
#else
      return "lo";
#endif
    }

    bool
    Start(std::shared_ptr< Logic > logic, llarp_ev_loop_ptr loop, uint16_t port)
    {
      if(!link)
        return false;
      if(!link->Configure(loop, localLoopBack(), AF_INET, port))
        return false;
      // if(!link->GenEphemeralKeys()) TODO: reimplement GenEphemeralKeys
        return false;
      rc.addrs.emplace_back();
      if(!link->GetOurAddressInfo(rc.addrs[0]))
        return false;
      if(!rc.Sign(signingKey))
        return false;
      return link->Start(logic, worker);
    }

    void
    Stop()
    {
      if(link)
        link->Stop();
      if(worker)
      {
        worker->drain();
        worker->stop();
      }
    }

    void
    TearDown()
    {
      link.reset();
      worker.reset();
    }
  };

  Context Alice;
  Context Bob;

  bool success = false;
  const bool shouldDebug = false;

  llarp_ev_loop_ptr netLoop;
  std::shared_ptr< Logic > m_logic;

  llarp_time_t oldRCLifetime;
  llarp::LogLevel oldLevel;

  LinkLayerTest() : netLoop(nullptr)
  {
  }

  void
  SetUp()
  {
    oldLevel = llarp::LogContext::Instance().minLevel;
    if(shouldDebug)
      llarp::SetLogLevel(eLogTrace);
    oldRCLifetime              = RouterContact::Lifetime;
    RouterContact::BlockBogons = false;
    RouterContact::Lifetime    = 500;
    netLoop                    = llarp_make_ev_loop();
    m_logic.reset(new Logic());
    Alice.Setup();
    Bob.Setup();
  }

  void
  TearDown()
  {
    Alice.TearDown();
    Bob.TearDown();
    m_logic.reset();
    netLoop.reset();
    RouterContact::BlockBogons = true;
    RouterContact::Lifetime    = oldRCLifetime;
    llarp::SetLogLevel(oldLevel);
  }

  void
  RunMainloop()
  {
    m_logic->call_later(5000, std::bind(&LinkLayerTest::Stop, this));
    llarp_ev_loop_run_single_process(netLoop, m_logic);
  }

  void
  Stop()
  {
    Alice.Stop();
    Bob.Stop();
    llarp_ev_loop_stop(netLoop);
    m_logic->stop();
  }
};

TEST_F(LinkLayerTest, TestIWP)
{
#ifdef WIN32
    GTEST_SKIP();
#else
    /*
     * TODO: use KeyManager
     *
    auto sendDiscardMessage = [](ILinkSession* s, auto callback) -> bool {
    // send discard message in reply to complete unit test
      std::vector< byte_t> tmp(32);
      llarp_buffer_t otherBuf(tmp);
      DiscardMessage discard;
      if(!discard.BEncode(&otherBuf))
        return false;
      return s->SendMessageBuffer(std::move(tmp), callback);
    };
    Alice.link = iwp::NewInboundLink(
      Alice.encryptionKey,
      [&]() -> const RouterContact& { return Alice.GetRC(); },
      [&](ILinkSession* s, const llarp_buffer_t& buf) -> bool {
          llarp_buffer_t copy(buf.base, buf.sz);
          if(not Alice.gotLIM)
          {
            LinkIntroMessage msg;
            if(msg.BDecode(&copy))
            { 
              Alice.gotLIM = s->GotLIM(&msg);
            }
          }
          return Alice.gotLIM;
      },
      [&](Signature& sig, const llarp_buffer_t& buf) -> bool {
        return m_crypto.sign(sig, Alice.signingKey, buf);
      },
      [&](ILinkSession* s) -> bool {
        const auto rc = s->GetRemoteRC();
        if(rc.pubkey != Bob.GetRC().pubkey)
          return false;
        LogInfo("alice established with bob");
        Alice.madeSession = true;
        sendDiscardMessage(s, [&](auto status) {
          success = status == llarp::ILinkSession::DeliveryStatus::eDeliverySuccess;
          LogInfo("message sent to bob suceess=", success);
          Stop();
        });
        return true;
      },
      [&](RouterContact, RouterContact) -> bool { return true; },
     
      [&](ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
        Stop();
      },
      [&](RouterID router) { ASSERT_EQ(router, Alice.GetRouterID()); },
      []() {})
      ;

  

  Bob.link = iwp::NewInboundLink(
      Bob.encryptionKey, [&]() -> const RouterContact& { return Bob.GetRC(); },
      [&](ILinkSession* s, const llarp_buffer_t& buf) -> bool {
          
          llarp_buffer_t copy(buf.base, buf.sz);
          if(not Bob.gotLIM)
          {
            LinkIntroMessage msg;
            if(msg.BDecode(&copy))
            { 
              Bob.gotLIM = s->GotLIM(&msg);
            }
            return Bob.gotLIM;
          }
          DiscardMessage discard;
          if(discard.BDecode(&copy))
          {
            LogInfo("bog got discard message from alice");
            return true;
          }
          return false;
      },

      [&](Signature& sig, const llarp_buffer_t& buf) -> bool {
        return m_crypto.sign(sig, Bob.signingKey, buf);
      },
      [&](ILinkSession* s) -> bool {
        if(s->GetRemoteRC().pubkey != Alice.GetRC().pubkey)
          return false;
        LogInfo("bob established with alice");
        Bob.madeSession = true;
        
        return true;
      },
      [&](RouterContact newrc, RouterContact oldrc) -> bool {
        return newrc.pubkey == oldrc.pubkey;
      },
      [&](ILinkSession* session) { ASSERT_FALSE(session->IsEstablished()); },
      [&](RouterID router) { ASSERT_EQ(router, Alice.GetRouterID()); },
      []() {})
      ;

  ASSERT_TRUE(Alice.Start(m_logic, netLoop, AlicePort));
  ASSERT_TRUE(Bob.Start(m_logic, netLoop, BobPort));

  LogicCall(m_logic, [&]() { ASSERT_TRUE(Alice.link->TryEstablishTo(Bob.GetRC())); });

  RunMainloop();
  ASSERT_TRUE(Alice.IsGucci());
  ASSERT_TRUE(Bob.IsGucci());
  ASSERT_TRUE(success);
  */
    ASSERT_TRUE(false); // FIXME, see above
#endif
};
