#ifndef LLARP_HANDLERS_EXIT_HPP
#define LLARP_HANDLERS_EXIT_HPP

#include <llarp/handlers/tun.hpp>
#include <llarp/exit/endpoint.hpp>
#include <unordered_map>

namespace llarp
{
  namespace handlers
  {
    struct ExitEndpoint final : public TunEndpoint
    {
      ExitEndpoint(const std::string& name, llarp_router* r);
      ~ExitEndpoint();

      void
      Tick(llarp_time_t now) override;

      bool
      SetOption(const std::string& k, const std::string& v) override;

      virtual std::string
      Name() const override;

     protected:
      void
      FlushSend();

     private:
     
      std::string m_Name;

      std::unordered_multimap< llarp::PubKey, llarp::exit::Endpoint,
                               llarp::PubKey::Hash >
          m_ActiveExits;

      std::unordered_map< llarp::huint32_t, llarp::PubKey,
                          llarp::huint32_t::Hash >
          m_AddrsToPubKey;
    };
  }  // namespace handlers
}  // namespace llarp
#endif