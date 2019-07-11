#include <crypto/crypto_libsodium.hpp>
#include <ev/ev.h>
#include <ev/pipe.hpp>
#include <util/aligned.hpp>
#include <util/logic.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <functional>

using TestPipeReadFunc = std::function< bool(const llarp_buffer_t) >;

struct EventLoopTest : public ::testing::Test
{
  llarp_ev_loop_ptr loop;
  std::shared_ptr< llarp::Logic > _logic;
  llarp::sodium::CryptoLibSodium crypto;

  static void
  OnTimeout(void* u, uint64_t, uint64_t left)
  {
    if(left)
      return;
    static_cast< EventLoopTest* >(u)->StopFail();
  }

  void
  SetUp()
  {
    loop   = llarp_make_ev_loop();
    _logic = std::make_shared< llarp::Logic >();
    _logic->call_later({10000, this, &OnTimeout});
  }

  void
  StopFail()
  {
    Stop();
    ASSERT_FALSE(true);
  }

  void
  Stop()
  {
    llarp_ev_loop_stop(loop);
  }

  void
  TearDown()
  {
    Stop();
    loop.reset();
    _logic.reset();
  }

  void
  RunLoop()
  {
    llarp_ev_loop_run_single_process(loop, _logic->thread, _logic);
  }
};

TEST_F(EventLoopTest, PipeWriteOne)
{
#ifdef _WIN32
  llarp::AlignedBuffer< 32 > data;
  data.Randomize();
  llarp_buffer_t other(data);

  struct TestPipe : public llarp_ev_pkt_pipe
  {
    const llarp_buffer_t& other;
    std::function< void(void) > stop;
    TestPipe(llarp_buffer_t& buf, llarp_ev_loop_ptr l,
             std::function< void(void) > _stop)
        : llarp_ev_pkt_pipe(l), other(buf), stop(_stop)
    {
    }
    void
    OnRead(const llarp_buffer_t& buf) override
    {
      ASSERT_EQ(buf.sz, other.sz);
      ASSERT_EQ(memcmp(buf.base, other.base, other.sz), 0);
      stop();
    }
  };

  TestPipe* testpipe = new TestPipe(other, loop, [&]() { Stop(); });
  ASSERT_TRUE(testpipe->Start());
  ASSERT_TRUE(loop->add_ev(testpipe, false));
  ASSERT_TRUE(testpipe->Write(other));
  RunLoop();
#else
  SUCCEED();
#endif
}

TEST_F(EventLoopTest, PipeWrite1K)
{
#ifdef _WIN32
  struct TestPipe : public llarp_ev_pkt_pipe
  {
    using Data_t = std::vector< llarp::AlignedBuffer< 1500 > >;
    Data_t data;
    size_t idx = 0;
    std::function< void(void) > stop;
    TestPipe(const size_t num, llarp_ev_loop_ptr l,
             std::function< void(void) > _stop)
        : llarp_ev_pkt_pipe(l), stop(_stop)
    {
      data.resize(num);
      for(auto& d : data)
        d.Randomize();
    }
    void
    OnRead(const llarp_buffer_t& buf) override
    {
      llarp_buffer_t other(data[idx]);
      ASSERT_EQ(buf.sz, other.sz);
      ASSERT_EQ(memcmp(buf.base, other.base, other.sz), 0);
      ++idx;
      if(idx < data.size())
        PumpIt();
      else
        stop();
    }

    void
    PumpIt()
    {
      llarp_buffer_t buf(data[idx]);
      ASSERT_TRUE(Write(buf));
    }
  };

  TestPipe* testpipe = new TestPipe(1000, loop, [&]() { Stop(); });
  ASSERT_TRUE(testpipe->Start());
  ASSERT_TRUE(loop->add_ev(testpipe, false));
  testpipe->PumpIt();
  RunLoop();
#else
  SUCCEED();
#endif
}
