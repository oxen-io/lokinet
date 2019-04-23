#include "logger.hpp"

extern "C"
void
llarp_enable_debug_mode() {
    llarp::SetLogLevel(llarp::eLogDebug);
}
