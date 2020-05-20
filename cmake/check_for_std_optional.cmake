# check for std::optional because macos is broke af sometimes

set(std_optional_code [[
#include <optional>

int main() {
   std::optional<int> maybe;
   maybe = 1;
   return *maybe == 1;
}
]])

check_cxx_source_compiles("${std_optional_code}" was_compiled)
if(was_compiled)
  message(STATUS "we have std::optional")
else()
  message(FATAL_ERROR "we dont have std::optional your compiler is broke af")
endif()
