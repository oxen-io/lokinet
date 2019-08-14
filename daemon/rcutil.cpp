#include <router_contact.hpp>
#include <util/logger.hpp>
#include <util/ostream_logger.hpp>

#include <absl/synchronization/mutex.h>
#include <cxxopts.hpp>
#include <string>
#include <vector>

bool
dumpRc(const std::vector< std::string >& files, bool json)
{
  nlohmann::json result;
  for(const auto& file : files)
  {
    llarp::RouterContact rc;
    const bool ret = rc.Read(file.c_str());

    if(ret)
    {
      if(json)
      {
        result[file] = rc.ToJson();
      }
      else
      {
        std::cout << "file = " << file << "\n";
        std::cout << rc << "\n\n";
      }
    }
    else
    {
      std::cerr << "file = " << file << " was not a valid rc file\n";
    }
  }

  if(json)
    std::cout << result << "\n";

  return true;
}

int
main(int argc, char* argv[])
{
#ifdef LOKINET_DEBUG
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
#endif

  // clang-format off
  cxxopts::Options options(
    "lokinet-rcutil",
    "LokiNET is a free, open source, private, decentralized, \"market based sybil resistant\" and IP based onion routing network"
  );

  options.add_options()
      ("v,verbose", "Verbose", cxxopts::value<bool>())
      ("h,help", "help", cxxopts::value<bool>())
      ("j,json", "output in json", cxxopts::value<bool>())
      ("dump", "dump rc file", cxxopts::value<std::vector<std::string> >(), "FILE");
  // clang-format on

  try
  {
    const auto result = options.parse(argc, argv);

    const bool json = result["json"].as< bool >();

    if(result.count("verbose") > 0)
    {
      SetLogLevel(llarp::eLogDebug);
      llarp::LogContext::Instance().logStream =
          std::make_unique< llarp::OStreamLogStream >(std::cerr);
      llarp::LogDebug("debug logging activated");
    }

    if(result.count("help") > 0)
    {
      std::cout << options.help() << std::endl;
      return 0;
    }

    if(result.count("dump") > 0)
    {
      if(!dumpRc(result["dump"].as< std::vector< std::string > >(), json))
      {
        return 1;
      }
    }
  }
  catch(const cxxopts::OptionParseException& ex)
  {
    std::cerr << ex.what() << std::endl;
    std::cout << options.help() << std::endl;
    return 1;
  }

  return 0;
}
