#include "lns_tracker.hpp"

namespace llarp::service
{
  std::function<void(std::optional<Address>)>
  LNSLookupTracker::MakeResultHandler(
      std::string name,
      std::size_t numPeers,
      std::function<void(std::optional<Address>)> resultHandler)
  {
    m_PendingLookups.emplace(name, LookupInfo{numPeers, resultHandler});
    return [name, this](std::optional<Address> found) {
      auto itr = m_PendingLookups.find(name);
      if (itr != m_PendingLookups.end() and itr->second.HandleOneResult(found))
        m_PendingLookups.erase(itr);
    };
  }

  bool
  LNSLookupTracker::LookupInfo::HandleOneResult(std::optional<Address> result)
  {
    if (result)
    {
      m_CurrentValues.insert(*result);
    }
    m_ResultsGotten++;
    if (m_ResultsGotten == m_ResultsNeeded)
    {
      if (m_CurrentValues.size() == 1)
      {
        m_HandleResult(*m_CurrentValues.begin());
      }
      else
      {
        m_HandleResult(std::nullopt);
      }
      return true;
    }
    return false;
  }
}  // namespace llarp::service
