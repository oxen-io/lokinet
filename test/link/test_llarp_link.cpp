#include <link/utp.hpp>
#include <link/iwp.hpp>
#include <ev/ev.h>
#include <messages/link_intro.hpp>
#include <messages/discard.hpp>

#include <crypto/crypto_libsodium.hpp>

#include <gtest/gtest.h>

struct LinkLayerTest : public ::testing::Test
{
  static constexpr uint16_t AlicePort = 5000;
  static constexpr uint16_t BobPort   = 6000;

  struct Context
  {
    Context(llarp::Crypto& c)
    {
      crypto = &c;
      crypto->identity_keygen(signingKey);
      crypto->encryption_keygen(encryptionKey);
      rc.pubkey = llarp::seckey_topublic(signingKey);
      rc.enckey = llarp::seckey_topublic(encryptionKey);
    }

    llarp::SecretKey signingKey;
    llarp::SecretKey encryptionKey;

    llarp::RouterContact rc;

    llarp::Crypto* crypto;

    bool gotLIM = false;

    const llarp::RouterContact&
    GetRC() const
    {
      return rc;
    }

    llarp::RouterID
    GetRouterID() const
    {
      return rc.pubkey;
    }

    /// regenerate rc and rotate onion key
    bool
    Regen()
    {
      crypto->encryption_keygen(encryptionKey);
      rc.enckey = llarp::seckey_topublic(encryptionKey);
      return rc.Sign(crypto, signingKey);
    }

    std::unique_ptr< llarp::ILinkLayer > link;

    static std::string
    localLoopBack()
    {
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
      return "lo0";
#else
      return "lo";
#endif
    }

    bool
    Start(llarp::Logic* logic, llarp_ev_loop* loop, uint16_t port)
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
      if(!rc.Sign(crypto, signingKey))
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

  llarp::sodium::CryptoLibSodium crypto;

  Context Alice;
  Context Bob;

  bool success = false;

  llarp_ev_loop* netLoop;
  std::unique_ptr< llarp::Logic > logic;

  llarp_time_t oldRCLifetime;

  LinkLayerTest() : Alice(crypto), Bob(crypto), netLoop(nullptr)
  {
  }

  void
  SetUp()
  {
    oldRCLifetime                      = llarp::RouterContact::Lifetime;
    llarp::RouterContact::IgnoreBogons = true;
    llarp::RouterContact::Lifetime     = 500;
    llarp_ev_loop_alloc(&netLoop);
    logic.reset(new llarp::Logic());
  }

  void
  TearDown()
  {
    Alice.TearDown();
    Bob.TearDown();
    logic.reset();
    llarp_ev_loop_free(&netLoop);
    llarp::RouterContact::IgnoreBogons = false;
    llarp::RouterContact::Lifetime     = oldRCLifetime;
  }

  static void
  OnTimeout(void* u, uint64_t, uint64_t left)
  {
    if(left)
      return;
    static_cast< LinkLayerTest* >(u)->Stop();
  }

  void
  RunMainloop()
  {
    logic->call_later({5000, this, &OnTimeout});
    llarp_ev_loop_run_single_process(netLoop, logic->thread, logic.get());
  }

  void
  Stop()
  {
    llarp_ev_loop_stop(netLoop);
  }

  bool AliceGotMessage(llarp_buffer_t)
  {
    success = true;
    Stop();
    return true;
  }
};

TEST_F(LinkLayerTest, TestUTPAliceRenegWithBob)
{
  Alice.link = llarp::utp::NewServer(
      &crypto, Alice.encryptionKey,
      [&]() -> const llarp::RouterContact& { return Alice.GetRC(); },
      [&](llarp::ILinkSession* s, llarp_buffer_t buf) -> bool {
        if(Alice.gotLIM)
        {
          Alice.Regen();
          return s->RenegotiateSession();
        }
        else
        {
          llarp::LinkIntroMessage msg;
          if(!msg.BDecode(&buf))
            return false;
          if(!s->GotLIM(&msg))
            return false;
          Alice.gotLIM = true;
          return true;
        }
      },
      [&](llarp::RouterContact rc) {
        ASSERT_EQ(rc, Bob.GetRC());
        llarp::LogInfo("alice established with bob");
      },
      [&](llarp::RouterContact, llarp::RouterContact) -> bool { return true; },
      [&](llarp::Signature& sig, llarp_buffer_t buf) -> bool {
        return crypto.sign(sig, Alice.signingKey, buf);
      },
      [&](llarp::ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
        Stop();
      },
      [&](llarp::RouterID router) { ASSERT_EQ(router, Bob.GetRouterID()); });

  auto sendDiscardMessage = [](llarp::ILinkSession* s) -> bool {
    // send discard message in reply to complete unit test
    byte_t tmp[32] = {0};
    auto otherBuf  = llarp::StackBuffer< decltype(tmp) >(tmp);
    llarp::DiscardMessage discard;
    if(!discard.BEncode(&otherBuf))
      return false;
    otherBuf.sz  = otherBuf.cur - otherBuf.base;
    otherBuf.cur = otherBuf.base;
    return s->SendMessageBuffer(otherBuf);
  };

  Bob.link = llarp::utp::NewServer(
      &crypto, Bob.encryptionKey,
      [&]() -> const llarp::RouterContact& { return Bob.GetRC(); },
      [&](llarp::ILinkSession* s, llarp_buffer_t buf) -> bool {
        llarp::LinkIntroMessage msg;
        if(!msg.BDecode(&buf))
          return false;
        if(!s->GotLIM(&msg))
          return false;
        Bob.gotLIM = true;
        return sendDiscardMessage(s);
      },
      [&](llarp::RouterContact rc) {
        ASSERT_EQ(rc, Alice.GetRC());
        llarp::LogInfo("bob established with alice");
        Bob.link->VisitSessionByPubkey(Alice.GetRC().pubkey.as_array(),
                                       sendDiscardMessage);
      },
      [&](llarp::RouterContact newrc, llarp::RouterContact oldrc) -> bool {
        success = newrc.pubkey == oldrc.pubkey;
        return true;
      },
      [&](llarp::Signature& sig, llarp_buffer_t buf) -> bool {
        return crypto.sign(sig, Bob.signingKey, buf);
      },
      [&](llarp::ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
      },
      [&](llarp::RouterID router) { ASSERT_EQ(router, Alice.GetRouterID()); });

  ASSERT_TRUE(Alice.Start(logic.get(), netLoop, AlicePort));
  ASSERT_TRUE(Bob.Start(logic.get(), netLoop, BobPort));

  ASSERT_TRUE(Alice.link->TryEstablishTo(Bob.GetRC()));

  RunMainloop();
  ASSERT_TRUE(Alice.gotLIM);
  ASSERT_TRUE(Bob.gotLIM);
  ASSERT_TRUE(success);
}

TEST_F(LinkLayerTest, TestUTPAliceConnectToBob)
{
  Alice.link = llarp::utp::NewServer(
      &crypto, Alice.encryptionKey,
      [&]() -> const llarp::RouterContact& { return Alice.GetRC(); },
      [&](llarp::ILinkSession* s, llarp_buffer_t buf) -> bool {
        if(Alice.gotLIM)
        {
          return AliceGotMessage(buf);
        }
        else
        {
          llarp::LinkIntroMessage msg;
          if(!msg.BDecode(&buf))
            return false;
          if(!s->GotLIM(&msg))
            return false;
          Alice.gotLIM = true;
          return true;
        }
      },
      [&](llarp::RouterContact rc) {
        ASSERT_EQ(rc, Bob.GetRC());
        llarp::LogInfo("alice established with bob");
      },
      [&](llarp::RouterContact, llarp::RouterContact) -> bool { return true; },
      [&](llarp::Signature& sig, llarp_buffer_t buf) -> bool {
        return crypto.sign(sig, Alice.signingKey, buf);
      },
      [&](llarp::ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
        Stop();
      },
      [&](llarp::RouterID router) { ASSERT_EQ(router, Bob.GetRouterID()); });

  auto sendDiscardMessage = [](llarp::ILinkSession* s) -> bool {
    // send discard message in reply to complete unit test
    byte_t tmp[32] = {0};
    auto otherBuf  = llarp::StackBuffer< decltype(tmp) >(tmp);
    llarp::DiscardMessage discard;
    if(!discard.BEncode(&otherBuf))
      return false;
    otherBuf.sz  = otherBuf.cur - otherBuf.base;
    otherBuf.cur = otherBuf.base;
    return s->SendMessageBuffer(otherBuf);
  };

  Bob.link = llarp::utp::NewServer(
      &crypto, Bob.encryptionKey,
      [&]() -> const llarp::RouterContact& { return Bob.GetRC(); },
      [&](llarp::ILinkSession* s, llarp_buffer_t buf) -> bool {
        llarp::LinkIntroMessage msg;
        if(!msg.BDecode(&buf))
          return false;
        if(!s->GotLIM(&msg))
          return false;
        Bob.gotLIM = true;
        return true;
      },
      [&](llarp::RouterContact rc) {
        ASSERT_EQ(rc, Alice.GetRC());
        llarp::LogInfo("bob established with alice");
        Bob.link->VisitSessionByPubkey(Alice.GetRC().pubkey.as_array(),
                                       sendDiscardMessage);
      },
      [&](llarp::RouterContact, llarp::RouterContact) -> bool { return true; },
      [&](llarp::Signature& sig, llarp_buffer_t buf) -> bool {
        return crypto.sign(sig, Bob.signingKey, buf);
      },
      [&](llarp::ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
      },
      [&](llarp::RouterID router) { ASSERT_EQ(router, Alice.GetRouterID()); });

  ASSERT_TRUE(Alice.Start(logic.get(), netLoop, AlicePort));
  ASSERT_TRUE(Bob.Start(logic.get(), netLoop, BobPort));

  ASSERT_TRUE(Alice.link->TryEstablishTo(Bob.GetRC()));

  RunMainloop();
  ASSERT_TRUE(Alice.gotLIM);
  ASSERT_TRUE(Bob.gotLIM);
  ASSERT_TRUE(success);
}

TEST_F(LinkLayerTest, TestIWPAliceConnectToBob)
{
  /*
  Alice.link = llarp::iwp::NewServer(
      &crypto, Alice.encryptionKey,
      [&]() -> const llarp::RouterContact& { return Alice.GetRC(); },
      [&](llarp::ILinkSession* s, llarp_buffer_t buf) -> bool {
        if(Alice.gotLIM)
        {
          return AliceGotMessage(buf);
        }
        else
        {
          llarp::LinkIntroMessage msg;
          if(!msg.BDecode(&buf))
            return false;
          if(!s->GotLIM(&msg))
            return false;
          Alice.gotLIM = true;
          return true;
        }
      },
      [&](llarp::RouterContact rc) {
        ASSERT_EQ(rc, Bob.GetRC());
        llarp::LogInfo("alice established with bob");
      },
      [&](llarp::RouterContact, llarp::RouterContact) -> bool { return true; },
      [&](llarp::Signature& sig, llarp_buffer_t buf) -> bool {
        return crypto.sign(sig, Alice.signingKey, buf);
      },
      [&](llarp::ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
        Stop();
      },
      [&](llarp::RouterID router) { ASSERT_EQ(router, Bob.GetRouterID()); });

  auto sendDiscardMessage = [](llarp::ILinkSession* s) -> bool {
    // send discard message in reply to complete unit test
    byte_t tmp[32] = {0};
    auto otherBuf  = llarp::StackBuffer< decltype(tmp) >(tmp);
    llarp::DiscardMessage discard;
    if(!discard.BEncode(&otherBuf))
      return false;
    otherBuf.sz  = otherBuf.cur - otherBuf.base;
    otherBuf.cur = otherBuf.base;
    return s->SendMessageBuffer(otherBuf);
  };

  Bob.link = llarp::iwp::NewServer(
      &crypto, Bob.encryptionKey,
      [&]() -> const llarp::RouterContact& { return Bob.GetRC(); },
      [&](llarp::ILinkSession* s, llarp_buffer_t buf) -> bool {
        llarp::LinkIntroMessage msg;
        if(!msg.BDecode(&buf))
          return false;
        if(!s->GotLIM(&msg))
          return false;
        Bob.gotLIM = true;
        return true;
      },
      [&](llarp::RouterContact rc) {
        ASSERT_EQ(rc, Alice.GetRC());
        llarp::LogInfo("bob established with alice");
        Bob.link->VisitSessionByPubkey(Alice.GetRC().pubkey.as_array(),
                                       sendDiscardMessage);
      },
      [&](llarp::RouterContact, llarp::RouterContact) -> bool { return true; },
      [&](llarp::Signature& sig, llarp_buffer_t buf) -> bool {
        return crypto.sign(sig, Bob.signingKey, buf);
      },
      [&](llarp::ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
      },
      [&](llarp::RouterID router) { ASSERT_EQ(router, Alice.GetRouterID()); });

  ASSERT_TRUE(Alice.Start(logic.get(), netLoop, AlicePort));
  ASSERT_TRUE(Bob.Start(logic.get(), netLoop, BobPort));

  ASSERT_TRUE(Alice.link->TryEstablishTo(Bob.GetRC()));

  RunMainloop();
  ASSERT_TRUE(Alice.gotLIM);
  ASSERT_TRUE(Bob.gotLIM);
  ASSERT_TRUE(success);
  */
}
