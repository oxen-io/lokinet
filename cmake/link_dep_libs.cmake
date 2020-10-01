# Properly links a target to a list of library names by finding the given libraries.  Takes:
# - a target
# - a linktype (e.g. INTERFACE, PUBLIC, PRIVATE)
# - a library search path (or "" for defaults)
# - any number of library names
function(link_dep_libs target linktype libdirs)
  foreach(lib ${ARGN})
    find_library(link_lib-${lib} NAMES ${lib} PATHS ${libdirs})
    if(link_lib-${lib})
      target_link_libraries(${target} ${linktype} ${link_lib-${lib}})
    endif()
  endforeach()   
endfunction()
