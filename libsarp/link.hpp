#ifndef LIBSARP_LINK_HPP
#define LIBSARP_LINK_HPP
#include <netinet/in.h>
#include <sarp/crypto.h>
#include <cstdint>
#include <map>
#include <memory>

#include <sarp/ev.h>

namespace sarp
{
  struct Link;
  
  struct PeerSession
  {
    sockaddr_in6 remoteAddr;
    sarp_pubkey_t remotePubkey;
    sarp_sharedkey_t sessionKey;

    uint64_t lastRX;

    sarp_crypto * _crypto;
    
    enum State
    {
      eHandshake0,
      eHandshake1,
      eHandshake2,
      eHandshake3,
      eEstablished,
      eTimeout
    };

    State state;

    /** inbound session */
    PeerSession(sarp_crypto * crypto, sockaddr_in6 remote);

    /** outbound session */
    PeerSession(sarp_crypto * crypto, sockaddr_in6 remote, sarp_pubkey_t remotePubkey);

    PeerSession & operator=(const PeerSession & other);
    
    void SendTo(Link * link, const uint8_t * buff, std::size_t sz);

    void RecvFrom(const uint8_t * buff, std::size_t sz);
    
  };

  typedef std::unique_ptr<PeerSession> PeerSession_ptr;
  
  struct Link
  {
    typedef std::map<sockaddr_in6, PeerSession_ptr> Sessions;
 
    Sessions sessions;
    sarp_seckey_t transportSecKey;
    sarp_pubkey_t transportPubKey;

    sarp_crypto * _crypto;

    Link(sarp_crypto * crypto);
    
    sarp_udp_listener listener;
    
  };
}

#endif
