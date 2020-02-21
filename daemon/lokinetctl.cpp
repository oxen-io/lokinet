#include <config/config.hpp>
#include <router_contact.hpp>
#include <util/logging/logger.hpp>
#include <util/logging/ostream_logger.hpp>

#include <cxxopts.hpp>
#include <string>
#include <vector>

#ifdef WITH_CURL
#include <curl/curl.h>
#endif

namespace
{
  bool
  dumpRc(const std::vector< std::string >& files)
  {
    nlohmann::json result;
    for(const auto& file : files)
    {
      llarp::RouterContact rc;
      const bool ret = rc.Read(file.c_str());

      if(ret)
      {
        result[file] = rc.ToJson();
      }
      else
      {
        std::cerr << "file = " << file << " was not a valid rc file\n";
      }
    }
    std::cout << result << "\n";
    return true;
  }

#ifdef WITH_CURL

  size_t
  curlCallback(void* contents, size_t size, size_t nmemb, void* userp) noexcept
  {
    auto* str       = static_cast< std::string* >(userp);
    size_t realsize = size * nmemb;

    char* asChar = static_cast< char* >(contents);

    std::copy(asChar, asChar + realsize, std::back_inserter(*str));

    return realsize;
  }

  bool
  executeJsonRpc(const std::string& command, const std::string& configFile)
  {
    // Do init (on windows this will do socket initialisation)
    curl_global_init(CURL_GLOBAL_ALL);

    llarp::Config config;
    if(!config.Load(configFile.c_str()))
    {
      llarp::LogError("Failed to load from config file: ", configFile);
      return false;
    }

    if(!config.api.enableRPCServer())
    {
      llarp::LogError("Config does not have RPC enabled");
      return false;
    }

    std::string address = config.api.rpcBindAddr() + "/jsonrpc";

    const nlohmann::json request{{"method", command},
                                 {"params", nlohmann::json::object()},
                                 {"id", "foo"}};

    const std::string requestStr = request.dump();

    std::unique_ptr< curl_slist, void (*)(curl_slist*) > chunk(
        curl_slist_append(nullptr, "content-type: application/json"),
        &curl_slist_free_all);

    std::unique_ptr< CURL, void (*)(CURL*) > curl(curl_easy_init(),
                                                  &curl_easy_cleanup);
    curl_easy_setopt(curl.get(), CURLOPT_URL, address.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, requestStr.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, requestStr.size());
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, chunk.get());

    std::string result;
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curlCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &result);

    auto res = curl_easy_perform(curl.get());
    if(res != CURLE_OK)
    {
      llarp::LogError("Failed to curl endpoint, ", curl_easy_strerror(res));
      return false;
    }

    std::cout << result << "\n";

    return true;
  }
#endif
}  // namespace

int
main(int argc, char* argv[])
{
  // clang-format off
  cxxopts::Options options(
    "lokinetctl",
    "LokiNET is a free, open source, private, decentralized, \"market based sybil resistant\" and IP based onion routing network"
  );

  options.add_options()
      ("v,verbose", "Verbose", cxxopts::value<bool>())
      ("h,help", "help", cxxopts::value<bool>())
      ("c,config", "config file", cxxopts::value<std::string>()->default_value(llarp::GetDefaultConfigPath().string()))
#ifdef WITH_CURL
      ("j,jsonrpc", "hit json rpc endpoint", cxxopts::value<std::string>())
#endif
      ("dump", "dump rc file", cxxopts::value<std::vector<std::string> >(), "FILE");
  // clang-format on

  try
  {
    const auto result = options.parse(argc, argv);

    if(result.count("verbose") > 0)
    {
      SetLogLevel(llarp::eLogDebug);
      llarp::LogContext::Instance().logStream =
          std::make_unique< llarp::OStreamLogStream >(true, std::cerr);
      llarp::LogDebug("debug logging activated");
    }
    else
    {
      SetLogLevel(llarp::eLogError);
      llarp::LogContext::Instance().logStream =
          std::make_unique< llarp::OStreamLogStream >(true, std::cerr);
    }

    if(result.count("help") > 0)
    {
      std::cout << options.help() << std::endl;
      return 0;
    }

    if(result.count("dump") > 0)
    {
      if(!dumpRc(result["dump"].as< std::vector< std::string > >()))
      {
        return 1;
      }
    }

#ifdef WITH_CURL
    if(result.count("jsonrpc") > 0)
    {
      if(!executeJsonRpc(result["jsonrpc"].as< std::string >(),
                         result["config"].as< std::string >()))
      {
        return 1;
      }
    }
#endif
  }
  catch(const cxxopts::OptionParseException& ex)
  {
    std::cerr << ex.what() << std::endl;
    std::cout << options.help() << std::endl;
    return 1;
  }

  return 0;
}
