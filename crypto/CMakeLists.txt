
add_library(lokinet-cryptography
  libntrup/src/ntru.cpp
  libntrup/src/ref/randomsmall.c
  libntrup/src/ref/swap.c
  libntrup/src/ref/rq_round3.c
  libntrup/src/ref/rq_recip3.c
  libntrup/src/ref/small.c
  libntrup/src/ref/rq_mult.c
  libntrup/src/ref/randomweightw.c
  libntrup/src/ref/random32.c
  libntrup/src/ref/dec.c
  libntrup/src/ref/r3_mult.c
  libntrup/src/ref/r3_recip.c
  libntrup/src/ref/keypair.c
  libntrup/src/ref/rq_rounded.c
  libntrup/src/ref/enc.c
  libntrup/src/ref/int32_sort.c
  libntrup/src/ref/rq.c
)

target_include_directories(lokinet-cryptography PUBLIC libntrup/include)

# The avx implementation uses runtime CPU feature detection to enable itself, so we *always* want to
# compile it with avx2/fma support when supported by the compiler even if we aren't compiling with
# general AVX2 enabled.
set(NTRU_AVX_SRC
  libntrup/src/avx/randomsmall.c
  libntrup/src/avx/weight.c
  libntrup/src/avx/swap.c
  libntrup/src/avx/rq_round3.c
  libntrup/src/avx/rq_recip3.c
  libntrup/src/avx/small.c
  libntrup/src/avx/randomweightw.c
  libntrup/src/avx/dec.c
  libntrup/src/avx/r3_recip.c
  libntrup/src/avx/keypair.c
  libntrup/src/avx/rq_rounded.c
  libntrup/src/avx/mult.c
  libntrup/src/avx/enc.c
  libntrup/src/avx/int32_sort.c
  libntrup/src/avx/rq.c
  libntrup/src/avx/rq_mod3.c
)
if(NOT NATIVE_BUILD AND USE_AVX2)
  # Assume cxxflags are already enabling AVX2
  target_sources(lokinet-cryptography PRIVATE ${NTRU_AVX_SRC})
else()
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-mavx2 COMPILER_SUPPORTS_AVX2)
  check_cxx_compiler_flag(-mfma COMPILER_SUPPORTS_FMA)
  if(COMPILER_SUPPORTS_AVX2 AND COMPILER_SUPPORTS_FMA)
    target_sources(lokinet-cryptography PRIVATE ${NTRU_AVX_SRC})
    set_property(SOURCE ${NTRU_AVX_SRC} APPEND PROPERTY COMPILE_FLAGS "-mavx2 -mfma")
    message(STATUS "Building libntrup with runtime AVX2/FMA support")
  else()
    message(STATUS "Not building with libntrup runtime AVX2/FMA support (can't figure out how to compile with AVX2/FMA: -mavx2 -mfma didn't work)")
  endif()
endif()

enable_lto(lokinet-cryptography)
add_log_tag(lokinet-cryptography)

if(BUILD_SHARED_LIBS)
  install(TARGETS lokinet-cryptography LIBRARY DESTINATION lib)
endif()

if (WARNINGS_AS_ERRORS)
  target_compile_options(lokinet-cryptography PUBLIC -Wall -Wextra -Werror)
endif()


option(DOWNLOAD_SODIUM "Allow libsodium to be downloaded and built locally if not found on the system" OFF)

# Allow -DDOWNLOAD_SODIUM=FORCE to download without even checking for a local libsodium
if(NOT DOWNLOAD_SODIUM STREQUAL "FORCE")
  find_package(Sodium 1.0.18)
endif()

if(sodium_FOUND)
  target_link_libraries(lokinet-cryptography PUBLIC sodium)
elseif(DOWNLOAD_SODIUM)
  message(STATUS "Sodium >= 1.0.18 not found, but DOWNLOAD_SODIUM specified, so downloading it")
  include(DownloadLibSodium)
  target_link_libraries(lokinet-cryptography PUBLIC sodium_vendor)
else()
  message(FATAL_ERROR "Could not find libsodium >= 1.0.18; either install it on your system or use -DDOWNLOAD_SODIUM=ON to download and build an internal copy")
endif()

