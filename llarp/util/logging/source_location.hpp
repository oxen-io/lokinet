#pragma once

#ifdef __cpp_lib_source_location
#include <source_location>
namespace slns = std;
#else
#include <experimental/source_location>
namespace slns = std::experimental;
#endif
