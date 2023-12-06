#include "context.hpp"

#include <memory>
#include <stdexcept>

namespace llarp::exit
{
  Context::Context(Router* r) : router(r)
  {}
  Context::~Context() = default;

  void
  Context::Tick(llarp_time_t now)
  {
    {
      auto itr = _exits.begin();
      while (itr != _exits.end())
      {
        itr->second->Tick(now);
        ++itr;
      }
    }
    {
      auto itr = _closed.begin();
      while (itr != _closed.end())
      {
        if ((*itr)->ShouldRemove())
          itr = _closed.erase(itr);
        else
          ++itr;
      }
    }
  }

  void
  Context::stop()
  {
    auto itr = _exits.begin();
    while (itr != _exits.end())
    {
      itr->second->Stop();
      _closed.emplace_back(std::move(itr->second));
      itr = _exits.erase(itr);
    }
  }

  util::StatusObject
  Context::ExtractStatus() const
  {
    util::StatusObject obj{};
    auto itr = _exits.begin();
    while (itr != _exits.end())
    {
      obj[itr->first] = itr->second->ExtractStatus();
      ++itr;
    }
    return obj;
  }

  void
  Context::calculate_exit_traffic(TrafficStats& stats)
  {
    auto itr = _exits.begin();
    while (itr != _exits.end())
    {
      itr->second->CalculateTrafficStats(stats);
      ++itr;
    }
  }

  exit::Endpoint*
  Context::find_endpoint_for_path(const PathID_t& path) const
  {
    auto itr = _exits.begin();
    while (itr != _exits.end())
    {
      auto ep = itr->second->FindEndpointByPath(path);
      if (ep)
        return ep;
      ++itr;
    }
    return nullptr;
  }

  bool
  Context::obtain_new_exit(const PubKey& pk, const PathID_t& path, bool permitInternet)
  {
    auto itr = _exits.begin();
    while (itr != _exits.end())
    {
      if (itr->second->AllocateNewExit(pk, path, permitInternet))
        return true;
      ++itr;
    }
    return false;
  }

  std::shared_ptr<handlers::ExitEndpoint>
  Context::get_exit_endpoint(std::string name) const
  {
    if (auto itr = _exits.find(name); itr != _exits.end())
    {
      return itr->second;
    }
    return nullptr;
  }

  void
  Context::add_exit_endpoint(
      const std::string& name, const NetworkConfig& networkConfig, const DnsConfig& dnsConfig)
  {
    if (_exits.find(name) != _exits.end())
      throw std::invalid_argument{fmt::format("An exit with name {} already exists", name)};

    auto endpoint = std::make_unique<handlers::ExitEndpoint>(name, router);
    endpoint->Configure(networkConfig, dnsConfig);

    // add endpoint
    if (!endpoint->Start())
      throw std::runtime_error{fmt::format("Failed to start endpoint {}", name)};

    _exits.emplace(name, std::move(endpoint));
  }

}  // namespace llarp::exit
