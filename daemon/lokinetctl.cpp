#include <config/config.hpp>
#include <router_contact.hpp>
#include <util/logging/logger.hpp>
#include <util/logging/ostream_logger.hpp>

#include <cxxopts.hpp>
#include <string>
#include <vector>

namespace
{
  bool
  dumpRc(const std::vector<std::string>& files)
  {
    nlohmann::json result;
    for (const auto& file : files)
    {
      llarp::RouterContact rc;
      const bool ret = rc.Read(file.c_str());

      if (ret)
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

}  // namespace

int
main(int argc, char* argv[])
{
  cxxopts::Options options(
      "lokinetctl",
      "LokiNET is a free, open source, private, "
      "decentralized, \"market based sybil resistant\" "
      "and IP based onion routing network");

  options.add_options()("v,verbose", "Verbose", cxxopts::value<bool>())(
      "h,help", "help", cxxopts::value<bool>())(
      "c,config",
      "config file",
      cxxopts::value<std::string>()->default_value(llarp::GetDefaultConfigPath().string()))(
      "dump", "dump rc file", cxxopts::value<std::vector<std::string>>(), "FILE");

  try
  {
    const auto result = options.parse(argc, argv);

    if (result.count("verbose") > 0)
    {
      SetLogLevel(llarp::eLogDebug);
      llarp::LogContext::Instance().logStream =
          std::make_unique<llarp::OStreamLogStream>(true, std::cerr);
      llarp::LogDebug("debug logging activated");
    }
    else
    {
      SetLogLevel(llarp::eLogError);
      llarp::LogContext::Instance().logStream =
          std::make_unique<llarp::OStreamLogStream>(true, std::cerr);
    }

    if (result.count("help") > 0)
    {
      std::cout << options.help() << std::endl;
      return 0;
    }

    if (result.count("dump") > 0)
    {
      if (!dumpRc(result["dump"].as<std::vector<std::string>>()))
      {
        return 1;
      }
    }
  }
  catch (const cxxopts::OptionParseException& ex)
  {
    std::cerr << ex.what() << std::endl;
    std::cout << options.help() << std::endl;
    return 1;
  }

  return 0;
}
