# This adds a dependency as a "system" dep - e.g -isystem
function(target_link_libraries_system target)
  set(libs ${ARGN})
  foreach(lib ${libs})
    get_target_property(lib_include_dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(${target} SYSTEM PUBLIC ${lib_include_dirs})
    target_link_libraries(${target} PUBLIC ${lib})
  endforeach(lib)
endfunction()
