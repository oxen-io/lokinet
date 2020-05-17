# -flto
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_ENABLED OUTPUT ipo_error)
if(IPO_ENABLED)
  message(STATUS "LTO enabled")
else()
  message(WARNING "LTO not supported by compiler: ${ipo_error}")
endif()

function(enable_lto)
  if(IPO_ENABLED)
    set_target_properties(${ARGN} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endfunction()
