#include "async_key_exchange.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <utility>

namespace llarp
{
  namespace service
  {
    AsyncKeyExchange::AsyncKeyExchange(
        EventLoop_ptr l,
        ServiceInfo r,
        const Identity& localident,
        const PQPubKey& introsetPubKey,
        const Introduction& remote,
        IDataHandler* h,
        const ConvoTag& t,
        ProtocolType proto)
        : loop(std::move(l))
        , m_remote(std::move(r))
        , m_LocalIdentity(localident)
        , introPubKey(introsetPubKey)
        , remoteIntro(remote)
        , handler(h)
        , tag(t)
    {
      msg.proto = proto;
    }

    void
    AsyncKeyExchange::Result(
        std::shared_ptr<AsyncKeyExchange> self, std::shared_ptr<ProtocolFrame> frame)
    {
      // put values
      self->handler->PutSenderFor(self->msg.tag, self->m_remote, false);
      self->handler->PutCachedSessionKeyFor(self->msg.tag, self->sharedKey);
      self->handler->PutIntroFor(self->msg.tag, self->remoteIntro);
      self->handler->PutReplyIntroFor(self->msg.tag, self->msg.introReply);
      self->hook(frame);
    }

    void
    AsyncKeyExchange::Encrypt(
        std::shared_ptr<AsyncKeyExchange> self, std::shared_ptr<ProtocolFrame> frame)
    {
      // derive ntru session key component
      SharedSecret K;
      auto crypto = CryptoManager::instance();
      crypto->pqe_encrypt(frame->C, K, self->introPubKey);
      // randomize Nonce
      frame->N.Randomize();
      // compure post handshake session key
      // PKE (A, B, N)
      SharedSecret sharedSecret;
      path_dh_func dh_client = util::memFn(&Crypto::dh_client, crypto);
      if (!self->m_LocalIdentity.KeyExchange(dh_client, sharedSecret, self->m_remote, frame->N))
      {
        LogError("failed to derive x25519 shared key component");
      }
      std::array<byte_t, 64> tmp = {{0}};
      // K
      std::copy(K.begin(), K.end(), tmp.begin());
      // H (K + PKE(A, B, N))
      std::copy(sharedSecret.begin(), sharedSecret.end(), tmp.begin() + 32);
      crypto->shorthash(self->sharedKey, llarp_buffer_t(tmp));
      // set tag
      self->msg.tag = self->tag;
      // set sender
      self->msg.sender = self->m_LocalIdentity.pub;
      // set version
      self->msg.version = LLARP_PROTO_VERSION;
      // encrypt and sign
      if (frame->EncryptAndSign(self->msg, K, self->m_LocalIdentity))
        self->loop->call([self, frame] { AsyncKeyExchange::Result(self, frame); });
      else
      {
        LogError("failed to encrypt and sign");
      }
    }
  }  // namespace service
}  // namespace llarp
