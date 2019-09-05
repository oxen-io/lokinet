#ifndef LLARP_LINK_LAYER_HPP
#define LLARP_LINK_LAYER_HPP
#include <util/types.hpp>

#include <cstdlib>

constexpr size_t MAX_LINK_MSG_SIZE                = 8192;
constexpr llarp_time_t DefaultLinkSessionLifetime = 60 * 1000;
constexpr size_t MaxSendQueueSize                 = 1024;
#endif
