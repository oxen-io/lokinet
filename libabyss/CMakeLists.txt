add_library(abyss
  src/md5.cpp
  src/http.cpp
  src/client.cpp
  src/server.cpp)

target_include_directories(abyss PUBLIC include)
target_link_libraries(abyss PUBLIC lokinet-platform)
enable_lto(abyss)

# for freebsd
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  target_include_directories(abyss SYSTEM PUBLIC /usr/local/include)
endif()

if(BUILD_SHARED_LIBS)
  install(TARGETS abyss LIBRARY DESTINATION lib)
endif()

add_log_tag(abyss)
