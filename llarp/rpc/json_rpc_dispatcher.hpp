#ifndef LLARP_JSON_RPC_DISPATCHER_HPP
#define LLARP_JSON_RPC_DISPATCHER_HPP

#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace llarp
{
  namespace rpc
  {
    /**
     * Response wrapper for handlers to reply with a payload and also indicate
     * whether or not it is an error.
     */
    struct response_t
    {
      bool isError = false;
      nlohmann::json payload;
    };

    using DispatchHandler =
        std::function< response_t(const nlohmann::json& params) >;

    /**
     * JsonRpcDispatcher is a class that maps JSON-RPC method names to callback
     * handlers. This abstraction allows JSON-RPC 2.0 to be used across various
     * transports with ease and without opinions on the transport.
     */
    struct JsonRpcDispatcher
    {
      /**
       * Process an input string and dispatch if possible. The input will be
       * JSON-parsed and delegated to *process(const nlohmann::json&);*
       *
       * Upon error, a JSON object will be returned with error details filled
       * out.
       */
      nlohmann::json
      processRaw(const std::string& input) const;

      /**
       * Process a request as a JSON object. The object will be validated as a
       * JSON-RPC 2.0 object and then a dispatch will be made if a matching
       * handler is found.
       *
       * Upon error, a JSON object will be returned with error details filled
       * out.
       */
      nlohmann::json
      process(const nlohmann::json& obj) const;

      /**
       * Set the handler for a given JSON-RPC 2.0 "method".
       */
      void
      setHandler(const std::string& method, DispatchHandler handler);

      /**
       * Remove the handler for a given JSON-RPC 2.0 "method".
       */
      void
      removeHandler(const std::string& method);

      /**
       * Produce a JSON-RPC 2.0 error object
       */
      static nlohmann::json
      createJsonRpcErrorObject(int code, std::string message,
                               nlohmann::json data = nullptr);

      /**
       * Produce a JSON-RPC 2.0 response object.
       *
       * Per the spec, id must be present and should be exactly what was passed
       * in the request (or null if it wasn't provided).
       *
       * Additionally, either *error* or *result* should be provided, but not
       * both.
       */
      static nlohmann::json
      createJsonRpcResponseObject(nlohmann::json id, nlohmann::json result,
                                  nlohmann::json error = nullptr);

     private:
      std::unordered_map< std::string, DispatchHandler > m_handlers;
    };
  }  // namespace rpc
}  // namespace llarp

#endif
