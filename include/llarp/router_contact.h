#ifndef LLARP_RC_H
#define LLARP_RC_H
#include <llarp/address_info.h>
#include <llarp/crypto.h>
#include <llarp/exit_info.h>

#ifdef __cplusplus
#include <string>
#endif

// forward declare
struct llarp_alloc;
struct llarp_rc;

#define MAX_RC_SIZE (1024)
#define NICKLEN (32)

bool
llarp_rc_bdecode(struct llarp_rc *rc, llarp_buffer_t *buf);
bool
llarp_rc_bencode(const struct llarp_rc *rc, llarp_buffer_t *buf);

struct llarp_rc
{
  struct llarp_ai_list *addrs;
  // public encryption public key
  byte_t enckey[PUBKEYSIZE];
  // public signing public key
  byte_t pubkey[PUBKEYSIZE];
  struct llarp_xi_list *exits;
  byte_t signature[SIGSIZE];
  /// node nickname, yw kee
  byte_t nickname[NICKLEN];

  uint64_t last_updated;

#ifdef __cplusplus
  bool
  BEncode(llarp_buffer_t *buf) const
  {
    return llarp_rc_bencode(this, buf);
  }

  bool
  BDecode(llarp_buffer_t *buf)
  {
    return llarp_rc_bdecode(this, buf);
  }

  std::string
  Nick() const
  {
    const char *nick = (const char *)nickname;
    return std::string(nick, strnlen(nick, sizeof(nickname)));
  }

#endif
};

void
llarp_rc_free(struct llarp_rc *rc);

bool
llarp_rc_new(struct llarp_rc *rc);

bool
llarp_rc_verify_sig(struct llarp_crypto *crypto, struct llarp_rc *rc);

void
llarp_rc_copy(struct llarp_rc *dst, const struct llarp_rc *src);

bool
llarp_rc_is_public_router(const struct llarp_rc *const rc);

void
llarp_rc_set_addrs(struct llarp_rc *rc, struct llarp_alloc *mem,
                   struct llarp_ai_list *addr);

bool
llarp_rc_set_nickname(struct llarp_rc *rc, const char *name);

void
llarp_rc_set_pubenckey(struct llarp_rc *rc, const uint8_t *pubenckey);

void
llarp_rc_set_pubsigkey(struct llarp_rc *rc, const uint8_t *pubkey);

/// combo
void
llarp_rc_set_pubkey(struct llarp_rc *rc, const uint8_t *pubenckey,
                    const uint8_t *pubsigkey);

void
llarp_rc_sign(struct llarp_crypto *crypto, const byte_t *seckey,
              struct llarp_rc *rc);

void
llarp_rc_clear(struct llarp_rc *rc);

bool
llarp_rc_addr_list_iter(struct llarp_ai_list_iter *iter, struct llarp_ai *ai);

bool
llarp_rc_read(const char *fpath, struct llarp_rc *result);

bool
llarp_rc_write(struct llarp_rc *rc, const char *our_rc_file);

#endif
