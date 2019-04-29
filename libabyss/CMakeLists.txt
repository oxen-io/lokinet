add_library(${ABYSS_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/src/md5.cpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/src/http.cpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/src/client.cpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/src/server.cpp")

target_include_directories(${ABYSS_LIB} PUBLIC include)
target_link_libraries(${ABYSS_LIB} PUBLIC ${PLATFORM_LIB})
