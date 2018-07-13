#ifndef LLARP_API_MESSAGES_HPP
#define LLARP_API_MESSAGES_HPP

#include <list>
#include <llarp/aligned.hpp>
#include <llarp/bencode.hpp>
#include <llarp/crypto.hpp>
#include <llarp/service/Info.hpp>

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
      uint64_t seqno = 0;
      llarp::ShortHash hash;

      virtual ~IMessage(){};

      // the function name this message belongs to
      virtual std::string
      FunctionName() const = 0;

      bool
      BEncode(llarp_buffer_t* buf) const;

      virtual bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

      /// encode the dictionary members after the A value and before the Y value
      virtual bool
      EncodeParams(llarp_buffer_t* buf) const = 0;

      bool
      IsWellFormed(llarp_crypto* c, const std::string& password);

      void
      CalculateHash(llarp_crypto* c, const std::string& password);
    };

    /// a "yes we got your command" type message
    struct AckMessage : public IMessage
    {
      ~AckMessage();

      bool
      EncodeParams(llarp_buffer_t* buf) const;

      std::string
      FunctionName() const
      {
        return "ack";
      }
    };

    // spawn hidden service message
    struct SpawnMessage : public IMessage
    {
      ~SpawnMessage();

      std::string SessionName;
      llarp::service::ServiceInfo Info;

      bool
      DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

      bool
      EncodeParams(llarp_buffer_t* buf) const;

      std::string
      FunctionName() const
      {
        return "spawn";
      }
    };

    /// a keepalive ping
    struct KeepAliveMessage : public IMessage
    {
      ~KeepAliveMessage();

      bool
      EncodeParams(llarp_buffer_t* buf) const;

      std::string
      FunctionName() const
      {
        return "keepalive";
      }
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