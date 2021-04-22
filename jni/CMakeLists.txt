add_library(lokinet-android
    SHARED
    lokinet_config.cpp
    lokinet_daemon.cpp)
add_log_tag(lokinet-android)
target_link_libraries(lokinet-android liblokinet)
