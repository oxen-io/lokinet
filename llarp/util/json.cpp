#include "json.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace llarp
{
  namespace json
  {
    struct NlohmannJSONParser : public IParser
    {
      NlohmannJSONParser(size_t contentSize) : m_Buf(contentSize + 1), m_Offset(0)
      {
        assert(contentSize != 0);
      }

      std::vector<char> m_Buf;
      size_t m_Offset;

      bool
      FeedData(const char* buf, size_t sz) override
      {
        if (sz == 0)
          return true;
        if (m_Offset + sz > m_Buf.size() - 1)
          return false;
        std::copy_n(buf, sz, m_Buf.data() + m_Offset);
        m_Offset += sz;
        m_Buf[m_Offset] = 0;
        return true;
      }

      Result
      Parse(nlohmann::json& obj) const override
      {
        if (m_Offset < m_Buf.size() - 1)
          return eNeedData;

        try
        {
          obj = nlohmann::json::parse(m_Buf.data());
          return eDone;
        }
        catch (const nlohmann::json::exception&)
        {
          return eParseError;
        }
      }
    };

    IParser*
    MakeParser(size_t contentSize)
    {
      return new NlohmannJSONParser(contentSize);
    }

  }  // namespace json
}  // namespace llarp
