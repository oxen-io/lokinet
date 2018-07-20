#ifndef LLARP_IWP_SENDQUEUE_HPP
#define LLARP_IWP_SENDQUEUE_HPP
#include <llarp/iwp/sendbuf.hpp>
#include <llarp/codel.hpp>

typedef llarp::util::CoDelQueue<sendbuf_t *, sendbuf_t::GetTime, sendbuf_t::PutTime,
sendbuf_t::Compare,
    llarp::util::DummyMutex, llarp::util::DummyLock> sendqueue_t;

#endif