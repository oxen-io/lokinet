
if(SHADOW)
    add_shadow_plugin(shadow-plugin main.cpp ${TRACY_ROOT}/TracyClient.cpp)
    target_link_libraries(shadow-plugin liblokinet)
    enable_lto(shadow-plugin)
else()
    add_executable(lokinet main.cpp)
    add_executable(lokinetctl lokinetctl.cpp)
    enable_lto(lokinet lokinetctl)

    if(TRACY_ROOT)
        target_sources(lokinet PRIVATE ${TRACY_ROOT}/TracyClient.cpp)
    endif()

    foreach(exe lokinet lokinetctl)
        if(WIN32 AND NOT MSVC_VERSION)
            target_sources(${exe} PRIVATE ../llarp/win32/version.rc)
            target_link_libraries(${exe} PRIVATE ws2_32 iphlpapi)
        elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
            target_link_directories(${exe} PRIVATE /usr/local/lib)
        endif()
        target_link_libraries(${exe} PRIVATE liblokinet)
        if(WITH_JEMALLOC)
          target_link_libraries(${exe} PUBLIC jemalloc)
        endif()
        target_compile_definitions(${exe} PRIVATE -DVERSIONTAG=${GIT_VERSION_REAL})
        add_log_tag(${exe})
    endforeach()

    install(TARGETS lokinet RUNTIME DESTINATION bin COMPONENT lokinet)
    if(WIN32)
        install(PROGRAMS ${CMAKE_SOURCE_DIR}/lokinet-bootstrap.ps1 DESTINATION bin COMPONENT lokinet)
    else()
        install(PROGRAMS ${CMAKE_SOURCE_DIR}/lokinet-bootstrap DESTINATION bin COMPONENT lokinet)
    endif()

    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        install(CODE "execute_process(COMMAND setcap cap_net_admin,cap_net_bind_service=+eip ${CMAKE_INSTALL_PREFIX}/bin/lokinet)")
    endif()

    if(CURL_FOUND)
        target_include_directories(lokinetctl PRIVATE ${CURL_INCLUDE_DIRS})
        target_link_libraries(lokinetctl PRIVATE ${CURL_LIBRARIES})
    endif(CURL_FOUND)


    install(PROGRAMS lokinet-vpn DESTINATION bin COMPONENT lokinet)
    
endif()
