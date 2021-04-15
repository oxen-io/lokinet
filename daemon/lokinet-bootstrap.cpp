#include <cpr/cpr.h>
#include <llarp/constants/files.hpp>
#include <llarp/constants/version.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/fs.hpp>
#include <llarp/router_contact.hpp>

#include <sstream>

#ifndef _WIN32
#include <openssl/x509.h>
#endif

namespace
{
  int
  exit_with_message(std::string msg, int exitcode)
  {
    std::cout << msg << std::endl;
    return exitcode;
  }
}  // namespace

int
main(int argc, char* argv[])
{
  std::string bootstrap_url{"https://seed.lokinet.org/lokinet.signed"};
  if (argc > 1)
  {
    bootstrap_url = argv[1];
  }
  cpr::Response resp =
#ifdef _WIN32
      cpr::Get(
          cpr::Url{bootstrap_url}, cpr::Header{{"User-Agent", std::string{llarp::VERSION_FULL}}});
#else
      cpr::Get(
          cpr::Url{bootstrap_url},
          cpr::Header{{"User-Agent", std::string{llarp::VERSION_FULL}}},
          cpr::Ssl(cpr::ssl::CaPath{X509_get_default_cert_dir()}));
#endif
  if (resp.status_code != 200)
  {
    return exit_with_message(
        "failed to fetch '" + bootstrap_url + "' HTTP " + std::to_string(resp.status_code), 1);
  }

  std::stringstream ss;
  ss << resp.text;

  std::string data{ss.str()};
  llarp_buffer_t buf{&data[0], data.size()};

  llarp::RouterContact rc;
  if (not rc.BDecode(&buf))
    return exit_with_message("invalid bootstrap data was fetched", 1);

  const auto path = llarp::GetDefaultBootstrap();
  if (not rc.Write(path))
    return exit_with_message("failed to write bootstrap file to " + path.string(), 1);

  const llarp::RouterID router{rc.pubkey};
  return exit_with_message("fetched bootstrap file for " + router.ToString(), 0);
}
