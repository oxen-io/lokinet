#include <rpc/json_rpc_dispatcher.hpp>

#include <util/logging/logger.hpp>

namespace llarp
{
  namespace rpc
  {
    constexpr int ERROR_PARSE            = -32700;
    constexpr int ERROR_INVALID_REQUEST  = -32600;
    constexpr int ERROR_METHOD_NOT_FOUND = -32601;
    constexpr int ERROR_INVALID_PARAMS   = -32602;
    constexpr int ERROR_INTERNAL         = -32603;
    constexpr int ERROR_SERVER           = -32000;

    nlohmann::json
    JsonRpcDispatcher::processRaw(const std::string& input) const
    {
      nlohmann::json obj;
      try
      {
        obj = nlohmann::json::parse(input);
      }
      catch(const nlohmann::json::exception& e)
      {
        LogWarn("Error parsing JSON string. Error: ", e.what());
        auto error = createJsonRpcErrorObject(
            ERROR_PARSE, "Invalid JSON string", {{"parseMessage", e.what()}});
        return createJsonRpcResponseObject(nullptr, nullptr, error);
      }

      return process(obj);
    }

    nlohmann::json
    JsonRpcDispatcher::process(const nlohmann::json& obj) const
    {
      // JSON-RPC 2.0 request should be a JSON object...
      if(obj.type() != nlohmann::json::value_t::object)
      {
        auto error = createJsonRpcErrorObject(
            ERROR_INVALID_REQUEST, "Request not a proper JSON object");
        return createJsonRpcResponseObject(nullptr, nullptr, error);
      }

      // id is optional, must be {string, number, NULL} if present, and we treat
      // as a null if if not present
      auto itr = obj.find("id");
      nlohmann::json id;  // effectively a type of 'null' at this point
      if(itr != obj.end())
      {
        id = itr.value();
        switch(id.type())
        {
          case nlohmann::json::value_t::null:
          case nlohmann::json::value_t::string:
          case nlohmann::json::value_t::number_integer:
          case nlohmann::json::value_t::number_unsigned:
            // acceptable values
            break;

          default:
            auto error = createJsonRpcErrorObject(ERROR_INVALID_REQUEST,
                                                  "Invalid id type");
            return createJsonRpcResponseObject(nullptr, nullptr, error);
        }
      }

      // JSON-RPC 2.0 request should identify itself as such
      itr = obj.find("jsonrpc");
      if(itr == obj.end())
      {
        auto error = createJsonRpcErrorObject(ERROR_INVALID_REQUEST,
                                              "Request missing 'jsonrpc'");
        return createJsonRpcResponseObject(id, nullptr, error);
      }
      nlohmann::json jsonrpc = itr.value();
      if(jsonrpc.type() != nlohmann::json::value_t::string || jsonrpc != "2.0")
      {
        auto error = createJsonRpcErrorObject(ERROR_INVALID_REQUEST,
                                              "Request must identify as "
                                              "\"jsonrpc\": \"2.0\"");
        return createJsonRpcResponseObject(id, nullptr, error);
      }

      // JSON-RPC 2.0 request should have a method parameter of type string
      itr = obj.find("method");
      if(itr == obj.end())
      {
        auto error = createJsonRpcErrorObject(ERROR_INVALID_REQUEST,
                                              "Request missing 'method'");
        return createJsonRpcResponseObject(id, nullptr, error);
      }
      auto method = itr.value();
      if(method.type() != nlohmann::json::value_t::string)
      {
        auto error = createJsonRpcErrorObject(
            ERROR_INVALID_REQUEST, "Request should contain method parameter");
        return createJsonRpcResponseObject(id, nullptr, error);
      }

      // TODO: params (optional: array or obj, should be passed to handler)

      std::string methodName = method;
      // TODO: validate: method must exist and should be a string

      auto handlersItr = m_handlers.find(methodName);
      if(handlersItr == m_handlers.end())
      {
        auto error =
            createJsonRpcErrorObject(ERROR_METHOD_NOT_FOUND, "Method not found",
                                     {{"method", methodName}});
        return createJsonRpcResponseObject(id, nullptr, error);
      }

      auto handler        = handlersItr->second;
      response_t response = handler(nullptr);

      if(response.isError)
      {
        auto error = createJsonRpcErrorObject(ERROR_INTERNAL, "Internal error",
                                              response.payload);
        return createJsonRpcResponseObject(id, nullptr, error);
      }
      else
      {
        return createJsonRpcResponseObject(id, response.payload);
      }
    }

    void
    JsonRpcDispatcher::setHandler(const std::string& method,
                                  DispatchHandler handler)
    {
      m_handlers[method] = std::move(handler);
    }

    void
    JsonRpcDispatcher::removeHandler(const std::string& method)
    {
      auto itr = m_handlers.find(method);
      if(itr != m_handlers.end())
      {
        m_handlers.erase(itr);
      }
    }

    nlohmann::json
    JsonRpcDispatcher::createJsonRpcErrorObject(int code, std::string message,
                                                nlohmann::json data)
    {
      nlohmann::json obj;
      obj.emplace("code", code);
      obj.emplace("message", std::move(message));
      if(data != nullptr)
        obj.emplace("data", std::move(data));

      return obj;
    }

    nlohmann::json
    JsonRpcDispatcher::createJsonRpcResponseObject(nlohmann::json id,
                                                   nlohmann::json result,
                                                   nlohmann::json error)
    {
      nlohmann::json obj;
      obj.emplace("id", std::move(id));
      obj.emplace("jsonrpc", "2.0");

      if(error != nullptr && result != nullptr)
      {
        LogWarn(
            "Warning: caller provided both error and result to "
            "createJsonRpcResponseObject(), ignoring result");
      }

      if(error != nullptr)
      {
        obj.emplace("error", std::move(error));
      }
      else
      {
        obj.emplace("result", std::move(result));
      }

      return obj;
    }

  }  // namespace rpc
}  // namespace llarp
