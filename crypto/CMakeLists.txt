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

set(NTRU_REF_SRC
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

set(NTRU_SRC
  ${NTRU_AVX_SRC}
  ${NTRU_REF_SRC}
  libntrup/src/ntru.cpp
  )

set(CRYPTOGRAPHY_SRC ${NTRU_SRC})

if(NOT VENDOR_LIBSODIUM)
  find_package(Sodium 1.0.16)
  if(NOT sodium_FOUND)
    set(VENDOR_LIBSODIUM ON)
    message(STATUS "Could not find libsodium >= 1.0.16, using vendored copy")
  endif()
endif()

if(VENDOR_LIBSODIUM)
set(SHA512_SRC
  sha512/sha512.c)

set(CHACHA_SRC
  chacha20/ref/chacha20_ref.c
  chacha20/dolbeau/chacha20_dolbeau-ssse3.c
  chacha20/dolbeau/chacha20_dolbeau-avx2.c
  chacha20/stream_chacha20.c
  salsa20/ref/salsa20_ref.c
  salsa20/core_salsa_ref.c
  salsa20/stream_salsa20.c
  salsa20/xmm6/salsa20_xmm6-asm.S
  salsa20/xmm6/salsa20_xmm6.c
  salsa20/xmm6int/salsa20_xmm6int-avx2.c
  salsa20/xmm6int/salsa20_xmm6int-sse2.c
  xchacha20/hchacha.c
  xchacha20/stream_xchacha20.c)

set(CSRNG_SRC
  csrng/randombytes_salsa20_random.c
  csrng/randombytes.c)

set(CRYPTO_MEM_SRC
  secmem/secmem.c)

set(BLAKE2B_SRC
    blake2b/blake2b-compress-avx2.c
    blake2b/blake2b-compress-ref.c
    blake2b/blake2b-compress-sse41.c
    blake2b/blake2b-compress-ssse3.c
    blake2b/blake2b-ref.c
    blake2b/generichash_blake2b.c)

set(X25519_SRC
  curve25519/crypto_scalarmult.c
  curve25519/ref10/x25519_ref10.c
  curve25519/ref10/ed25519_ref10.c
  curve25519/sandy2x/fe51_invert.c
  curve25519/sandy2x/ladder_base.S
  curve25519/sandy2x/curve25519_sandy2x.c
  curve25519/sandy2x/consts.S
  curve25519/sandy2x/fe51_nsquare.S
  curve25519/sandy2x/fe51_mul.S
  curve25519/sandy2x/fe51_pack.S
  curve25519/sandy2x/fe_frombytes_sandy2x.c
  curve25519/sandy2x/sandy2x.S
  curve25519/sandy2x/ladder.S
  curve25519/scalarmult_curve25519.c
  ed25519/crypto_box.c
  ed25519/crypto_sign.c
  ed25519/ref10/open.c
  ed25519/ref10/obsolete.c
  ed25519/ref10/keypair.c
  ed25519/ref10/sign.c
  ed25519/sign_ed25519.c)

set(CRYPTOGRAPHY_SRC ${CRYPTOGRAPHY_SRC}
  libsodium/init.c
  libsodium/runtime.c
  verify/crypto_verify.c
  ${CRYPTO_MEM_SRC}
  ${CSRNG_SRC}
  ${BLAKE2B_SRC}
  ${CHACHA_SRC}
  ${ED25519_SRC}
  ${X25519_SRC}
  ${SHA512_SRC})
endif()

add_library(${CRYPTOGRAPHY_LIB} STATIC ${CRYPTOGRAPHY_SRC})
add_log_tag(${CRYPTOGRAPHY_LIB})
target_include_directories(${CRYPTOGRAPHY_LIB} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libntrup/include")
if(VENDOR_LIBSODIUM)
  target_include_directories (${CRYPTOGRAPHY_LIB} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
else()
  target_include_directories(${CRYPTOGRAPHY_LIB} PUBLIC ${sodium_INCLUDE_DIR})
  target_link_libraries(${CRYPOGRAPHY_LIB} sodium)
endif()