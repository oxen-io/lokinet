
#include <stdint.h>

#include "../stream_salsa20.h"
#include <sodium/crypto_stream_salsa20.h>
#if defined(ANDROID) || defined(iOS)
extern struct crypto_stream_salsa20_implementation
    crypto_stream_salsa20_xmm6int_sse2_implementation;
#endif
