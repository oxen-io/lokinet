#include <service/async_key_exchange.hpp>

#include <crypto/crypto.hpp>
#include <crypto/types.hpp>
#include <util/logic.hpp>

namespace llarp
{
  namespace service
  {
    AsyncKeyExchange::AsyncKeyExchange(Logic* l, Crypto* c,
                                       const ServiceInfo& r,
                                       const Identity& localident,
                                       const PQPubKey& introsetPubKey,
                                       const Introduction& remote,
                                       IDataHandler* h, const ConvoTag& t)
        : logic(l)
        , crypto(c)
        , remote(r)
        , m_LocalIdentity(localident)
        , introPubKey(introsetPubKey)
        , remoteIntro(remote)
        , handler(h)
        , tag(t)
    {
    }

    void
    AsyncKeyExchange::Result(void* user)
    {
      AsyncKeyExchange* self = static_cast< AsyncKeyExchange* >(user);
      // put values
      self->handler->PutCachedSessionKeyFor(self->msg.tag, self->sharedKey);
      self->handler->PutIntroFor(self->msg.tag, self->remoteIntro);
      self->handler->PutSenderFor(self->msg.tag, self->remote);
      self->handler->PutReplyIntroFor(self->msg.tag, self->msg.introReply);
      self->hook(self->frame);
      delete self;
    }

    void
    AsyncKeyExchange::Encrypt(void* user)
    {
      AsyncKeyExchange* self = static_cast< AsyncKeyExchange* >(user);
      // derive ntru session key component
      SharedSecret K;
      self->crypto->pqe_encrypt(self->frame.C, K, self->introPubKey);
      // randomize Nonce
      self->frame.N.Randomize();
      // compure post handshake session key
      // PKE (A, B, N)
      SharedSecret sharedSecret;
      using namespace std::placeholders;
      path_dh_func dh_client =
          std::bind(&Crypto::dh_client, self->crypto, _1, _2, _3, _4);
      if(!self->m_LocalIdentity.KeyExchange(dh_client, sharedSecret,
                                            self->remote, self->frame.N))
      {
        LogError("failed to derive x25519 shared key component");
      }
      std::array< byte_t, 64 > tmp = {{0}};
      // K
      std::copy(K.begin(), K.end(), tmp.begin());
      // H (K + PKE(A, B, N))
      std::copy(sharedSecret.begin(), sharedSecret.end(), tmp.begin() + 32);
      self->crypto->shorthash(self->sharedKey, llarp_buffer_t(tmp));
      // set tag
      self->msg.tag = self->tag;
      // set sender
      self->msg.sender = self->m_LocalIdentity.pub;
      // set version
      self->msg.version = LLARP_PROTO_VERSION;
      // set protocol
      self->msg.proto = eProtocolTraffic;
      // encrypt and sign
      if(self->frame.EncryptAndSign(self->crypto, self->msg, K,
                                    self->m_LocalIdentity))
        self->logic->queue_job({self, &Result});
      else
      {
        LogError("failed to encrypt and sign");
        delete self;
      }
    }
  }  // namespace service
}  // namespace llarp
