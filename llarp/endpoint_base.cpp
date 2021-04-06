#include "endpoint_base.hpp"

#include "llarp/util/algorithm.hpp"

namespace llarp
{
  void
  EndpointBase::PutSRVRecord(dns::SRVData srv)
  {
    if (auto result = m_SRVRecords.insert(std::move(srv)); result.second)
    {
      SRVRecordsChanged();
    }
  }
  bool
  EndpointBase::DelSRVRecordIf(std::function<bool(const dns::SRVData&)> filter)
  {
    if (util::erase_if(m_SRVRecords, filter) > 0)
    {
      SRVRecordsChanged();
      return true;
    }
    return false;
  }

  std::set<dns::SRVData>
  EndpointBase::SRVRecords() const
  {
    std::set<dns::SRVData> set;
    set.insert(m_SRVRecords.begin(), m_SRVRecords.end());
    return set;
  }

}  // namespace llarp
