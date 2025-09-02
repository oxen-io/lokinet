#pragma once

// Header for making actual log statements such as llarp::log::Info and so on work.

#include <oxen/log.hpp>
#include <oxen/log/catlogger.hpp>
#include <oxen/log/ring_buffer_sink.hpp>

namespace llarp
{
    namespace log = oxen::log;

    // Special "global" category that defaults to info level logging if not specifically overridden
    // (i.e. even if using a warning or higher global log level).
    extern log::CategoryLogger log_global;

    extern std::shared_ptr<log::RingBufferSink> logRingBuffer;
}  // namespace llarp
