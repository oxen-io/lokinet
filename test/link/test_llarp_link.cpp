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

    SecretKey signingKey;
    SecretKey encryptionKey;

    RouterContact rc;

    bool gotLIM = false;

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
      if(!link->GenEphemeralKeys())
        return false;
      rc.addrs.emplace_back();
      if(!link->GetOurAddressInfo(rc.addrs[0]))
        return false;
      if(!rc.Sign(signingKey))
        return false;
      return link->Start(logic);
    }

    void
    Stop()
    {
      if(link)
        link->Stop();
    }

    void
    TearDown()
    {
      Stop();
      link.reset();
    }
  };

  Context Alice;
  Context Bob;

  bool success = false;

  llarp_ev_loop_ptr netLoop;
  std::shared_ptr< Logic > m_logic;

  llarp_time_t oldRCLifetime;

  LinkLayerTest() : netLoop(nullptr)
  {
  }

  void
  SetUp()
  {
    oldRCLifetime              = RouterContact::Lifetime;
    RouterContact::BlockBogons = false;
    RouterContact::Lifetime    = 500;
    netLoop                    = llarp_make_ev_loop();
    m_logic.reset(new Logic());
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
  }

  static void
  OnTimeout(void* u, uint64_t, uint64_t left)
  {
    if(left)
      return;
    llarp::LogInfo("timed out test");
    static_cast< LinkLayerTest* >(u)->Stop();
  }

  void
  RunMainloop()
  {
    m_logic->call_later({5000, this, &OnTimeout});
    llarp_ev_loop_run_single_process(netLoop, m_logic);
  }

  void
  Stop()
  {
    llarp_ev_loop_stop(netLoop);
  }

  bool
  AliceGotMessage(const llarp_buffer_t&)
  {
    success = true;
    Stop();
    return true;
  }
};

TEST_F(LinkLayerTest, TestIWP)
{
#ifdef WIN32
    GTEST_SKIP();
#else
    Alice.link = iwp::NewInboundLink(
      Alice.encryptionKey,
      [&]() -> const RouterContact& { return Alice.GetRC(); },
      [&](ILinkSession* s, const llarp_buffer_t& buf) -> bool {
        if(Alice.gotLIM)
        {
          Alice.Regen();
          return s->RenegotiateSession();
        }
        else
        {
          LinkIntroMessage msg;
          ManagedBuffer copy{buf};
          if(!msg.BDecode(&copy.underlying))
            return false;
          if(!s->GotLIM(&msg))
            return false;
          Alice.gotLIM = true;
          return true;
        }
      },
      [&](Signature& sig, const llarp_buffer_t& buf) -> bool {
        return m_crypto.sign(sig, Alice.signingKey, buf);
      },
      [&](ILinkSession* s) -> bool {
        const auto rc = s->GetRemoteRC();
        return rc.pubkey == Bob.GetRC().pubkey;
      },
      [&](RouterContact, RouterContact) -> bool { return true; },
     
      [&](ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
        Stop();
      },
      [&](RouterID router) { ASSERT_EQ(router, Bob.GetRouterID()); });

  auto sendDiscardMessage = [](ILinkSession* s) -> bool {
    // send discard message in reply to complete unit test
    std::array< byte_t, 32 > tmp;
    llarp_buffer_t otherBuf(tmp);
    DiscardMessage discard;
    if(!discard.BEncode(&otherBuf))
      return false;
    otherBuf.sz = otherBuf.cur - otherBuf.base;
    otherBuf.cur = otherBuf.base;
    return s->SendMessageBuffer(otherBuf, nullptr);
  };

  Bob.link = iwp::NewInboundLink(
      Bob.encryptionKey, [&]() -> const RouterContact& { return Bob.GetRC(); },
      [&](ILinkSession* s, const llarp_buffer_t& buf) -> bool {
        LinkIntroMessage msg;
        ManagedBuffer copy{buf};
        if(!msg.BDecode(&copy.underlying))
          return false;
        if(!s->GotLIM(&msg))
          return false;
        Bob.gotLIM = true;
        return sendDiscardMessage(s);
      },

      [&](Signature& sig, const llarp_buffer_t& buf) -> bool {
        return m_crypto.sign(sig, Bob.signingKey, buf);
      },
      [&](ILinkSession* s) -> bool {
        if(s->GetRemoteRC().pubkey != Alice.GetRC().pubkey)
          return false;
        LogInfo("bob established with alice");
        return Bob.link->VisitSessionByPubkey(Alice.GetRC().pubkey.as_array(),
                                              sendDiscardMessage);
      },
      [&](RouterContact newrc, RouterContact oldrc) -> bool {
        success = newrc.pubkey == oldrc.pubkey;
        return true;
      },
      [&](ILinkSession* session) { ASSERT_FALSE(session->IsEstablished()); },
      [&](RouterID router) { ASSERT_EQ(router, Alice.GetRouterID()); });

  ASSERT_TRUE(Alice.Start(m_logic, netLoop, AlicePort));
  ASSERT_TRUE(Bob.Start(m_logic, netLoop, BobPort));

  ASSERT_TRUE(Alice.link->TryEstablishTo(Bob.GetRC()));

  RunMainloop();
  ASSERT_TRUE(Bob.gotLIM);
#endif
};
