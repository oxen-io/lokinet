#ifndef LLARP_API_MESSAGES_HPP
#define LLARP_API_MESSAGES_HPP

#include <list>
#include <llarp/aligned.hpp>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>

namespace llarp
{
  namespace api
  {
    // forward declare
    struct Client;
    struct Server;

    /// base message
    struct IMessage : public IBEncodeMessage
    {
      uint64_t sessionID = 0;
      uint64_t msgID     = 0;
      uint64_t version   = 0;
      llarp::ShortHash hash;

      // the function name this message belongs to
      virtual std::string
      FunctionName() const = 0;

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

      virtual std::list< IBEncodeMessage* >
      GetParams() const = 0;

      virtual bool
      DecodeParams(llarp_buffer_t* buf) = 0;

      bool
      IsWellFormed(llarp_crypto* c, const std::string& password);

      void
      CalculateHash(llarp_crypto* c, const std::string& password);
    };

    /// a "yes we got your command" type message
    struct AcknoledgeMessage : public IMessage
    {
    };

    /// start a session with the router
    struct CreateSessionMessage : public IMessage
    {
      std::list< IBEncodeMessage* >
      GetParams() const
      {
        return {};
      }

      bool
      DecodeParams(llarp_buffer_t* buf);

      std::string
      FunctionName() const
      {
        return "CreateSession";
      }
    };

    /// a keepalive ping
    struct SessionPingMessage : public IMessage
    {
    };

    /// end a session with the router
    struct DestroySessionMessage : public IMessage
    {
    };

    /// base messgae type for hidden service control and transmission
    struct HSMessage : public IMessage
    {
      llarp::PubKey pubkey;
      llarp::Signature sig;

      /// validate signature on message (server side)
      bool
      SignatureIsValid(llarp_crypto* crypto) const;

      /// sign message using secret key (client side)
      bool
      SignMessge(llarp_crypto* crypto, byte_t* seckey);
    };

    /// create a new hidden service
    struct CreateServiceMessgae : public HSMessage
    {
    };

    /// end an already created hidden service we created
    struct DestroyServiceMessage : public HSMessage
    {
    };

    /// start lookup of another service's descriptor
    struct LookupServiceMessage : public IMessage
    {
    };

    /// publish our hidden service's descriptor
    struct PublishServiceMessage : public IMessage
    {
    };

    /// send pre encrypted data down a path we own
    struct SendPathDataMessage : public IMessage
    {
    };

  }  // namespace api
}  // namespace llarp

#endif