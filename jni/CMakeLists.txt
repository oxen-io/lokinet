add_library(lokinet-android
    SHARED
    lokinet_config.cpp
    lokinet_daemon.cpp)
target_link_libraries(lokinet-android lokinet-amalgum)
