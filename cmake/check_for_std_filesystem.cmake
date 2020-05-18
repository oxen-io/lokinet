# Figure out if we need -lstdc++fs or -lc++fs and add it to the `filesystem` interface, if needed
# (otherwise just leave it an empty interface library; linking to it will do nothing).  The former
# is needed for gcc before v9, and the latter with libc++ before llvm v9.  But this gets more
# complicated than just using the compiler, because clang on linux by default uses libstdc++, so
# we'll just give up and see what works.

add_library(filesystem INTERFACE)

# why does cmake insist on using C++14 for this
set(filesystem_code [[
#include <filesystem>

int main() {
    auto cwd = std::filesystem::current_path();
    return !cwd.string().empty();
}
]])

set(CMAKE_CXX_STANDARD 17)

check_cxx_source_compiles("${filesystem_code}" filesystem_compiled)
if(filesystem_compiled)
  message(STATUS "No extra link flag needed for std::filesystem")
  set(filesystem_is_good 1)
else()
  foreach(fslib stdc++fs c++fs)
    set(CMAKE_REQUIRED_LIBRARIES -l${fslib})
    check_cxx_source_compiles("${filesystem_code}" filesystem_compiled_${fslib})
    if (filesystem_compiled_${fslib})
      message(STATUS "Using -l${fslib} for std::filesystem support")
      target_link_libraries(filesystem INTERFACE ${fslib})
      set(filesystem_is_good 1)
      break()
    endif()
  endforeach()
endif()
unset(CMAKE_REQUIRED_LIBRARIES)
if(filesystem_is_good EQUAL 1)
  message(STATUS "we have std::filesystem")
else()
  message(FATAL_ERROR "we don't have std::filesystem")
endif()
