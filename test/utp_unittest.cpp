#include <gtest/gtest.h>
#include <link/utp_internal.hpp>
#include <messages/link_intro.hpp>
#include <messages/discard.hpp>
#include <ev.h>

struct UTPTest : public ::testing::Test
{
  using Link_t = llarp::utp::LinkLayer;

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

    std::unique_ptr< Link_t > link;

    bool
    Start(llarp::Logic* logic, llarp_ev_loop* loop, uint16_t port)
    {
      if(!link->Configure(loop, "lo", AF_INET, port))
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
      link->Stop();
    }

    void
    TearDown()
    {
      Stop();
      link.reset();
    }
  };

  llarp::Crypto crypto;

  Context Alice;
  Context Bob;

  bool success = false;

  llarp_ev_loop* netLoop;
  std::unique_ptr< llarp::Logic > logic;

  UTPTest()
      : crypto(llarp::Crypto::sodium{})
      , Alice(crypto)
      , Bob(crypto)
      , netLoop(nullptr)
  {
  }

  void
  SetUp()
  {
    llarp::RouterContact::IgnoreBogons = true;
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
  }

  static void
  OnTimeout(void* u, uint64_t, uint64_t left)
  {
    if(left)
      return;
    static_cast< UTPTest* >(u)->Stop();
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
    return true;
  }

  bool BobGotMessage(llarp_buffer_t)
  {
    success = true;
    return true;
  }
};

TEST_F(UTPTest, TestAliceAndBob)
{
  Alice.link.reset(new Link_t(
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
          return true;
        }
      },
      [&](llarp::Signature& sig, llarp_buffer_t buf) -> bool {
        return crypto.sign(sig, Alice.signingKey, buf);
      },
      [&](llarp::RouterContact rc) { ASSERT_EQ(rc, Bob.GetRC()); },
      [&](llarp::ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
        Stop();
      },
      [&](llarp::RouterID router) { ASSERT_EQ(router, Bob.GetRouterID()); }));

  Bob.link.reset(new Link_t(
      &crypto, Bob.encryptionKey,
      [&]() -> const llarp::RouterContact& { return Bob.GetRC(); },
      [&](llarp::ILinkSession* s, llarp_buffer_t buf) -> bool {
        if(Bob.gotLIM)
        {
          return BobGotMessage(buf);
        }
        else
        {
          llarp::LinkIntroMessage msg;
          if(!msg.BDecode(&buf))
            return false;
          if(!s->GotLIM(&msg))
            return false;
          Bob.gotLIM = true;
          return true;
        }
      },
      [&](llarp::Signature& sig, llarp_buffer_t buf) -> bool {
        return crypto.sign(sig, Bob.signingKey, buf);
      },
      [&](llarp::RouterContact rc) { ASSERT_EQ(rc, Alice.GetRC()); },
      [&](llarp::ILinkSession* session) {
        ASSERT_FALSE(session->IsEstablished());
      },
      [&](llarp::RouterID router) { ASSERT_EQ(router, Alice.GetRouterID()); }));

  ASSERT_TRUE(Alice.Start(logic.get(), netLoop, AlicePort));
  ASSERT_TRUE(Bob.Start(logic.get(), netLoop, BobPort));

  ASSERT_TRUE(Alice.link->TryEstablishTo(Bob.GetRC()));

  RunMainloop();
  ASSERT_TRUE(Alice.gotLIM);
  ASSERT_TRUE(Bob.gotLIM);
  ASSERT_TRUE(success);
}
