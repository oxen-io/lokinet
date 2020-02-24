#ifndef LLARP_LINK_LAYER_HPP
#define LLARP_LINK_LAYER_HPP
#include <util/types.hpp>
#include <util/time.hpp>

#include <cstdlib>

constexpr size_t MAX_LINK_MSG_SIZE               = 8192;
static constexpr auto DefaultLinkSessionLifetime = 1min;
constexpr size_t MaxSendQueueSize                = 1024;
#endif
