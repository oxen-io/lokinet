# libtuntap python binding CMakeLists.txt
# =======================================

find_package(PythonLibs)
find_package(Boost COMPONENTS system thread python)

if(${PYTHONLIBS_FOUND} AND ${Boost_FOUND})
    include_directories(${PYTHON_INCLUDE_PATH})
    include_directories(${Boost_INCLUDE_PATH})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/bindings/cpp)

    add_library(python-tuntap SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/pytuntap.cc
    )
    target_link_libraries(python-tuntap
        ${PYTHON_LIBRARIES}
        ${Boost_LIBRARIES}
        tuntap++
    )
    add_custom_command(TARGET python-tuntap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/bindings/python/__init__.py"
        "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/bindings/python/pytuntap"
    )
    set_target_properties(python-tuntap PROPERTIES OUTPUT_NAME "_pytuntap")
    set_target_properties(python-tuntap PROPERTIES PREFIX "")
    set_target_properties(python-tuntap
        PROPERTIES LIBRARY_OUTPUT_DIRECTORY
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/bindings/python/pytuntap
    )
endif()

