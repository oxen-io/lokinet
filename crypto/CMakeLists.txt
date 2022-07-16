
add_library(lokinet-cryptography
  STATIC
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

enable_lto(lokinet-cryptography)
add_log_tag(lokinet-cryptography)

if (WARNINGS_AS_ERRORS)
  target_compile_options(lokinet-cryptography PUBLIC -Wall -Wextra -Werror)
endif()

target_link_libraries(lokinet-cryptography PUBLIC sodium)

