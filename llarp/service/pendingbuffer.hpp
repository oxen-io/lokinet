#pragma once

#include "protocol.hpp"

#include <llarp/util/buffer.hpp>

#include <vector>

namespace llarp::service
{
    struct PendingBuffer
    {
        std::vector<byte_t> payload;
        ProtocolType protocol;

        inline llarp_buffer_t Buffer()
        {
            return llarp_buffer_t{payload};
        }

        PendingBuffer(const llarp_buffer_t& buf, ProtocolType t) : payload{buf.copy()}, protocol{t}
        {}
    };

}  // namespace llarp::service
