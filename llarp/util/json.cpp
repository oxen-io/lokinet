#include <util/json.hpp>
#include <util/string_view.hpp>

#include <cstring>
#include <string>
#include <vector>

namespace llarp
{
  namespace json
  {
    struct NlohmannJSONParser : public IParser
    {
      NlohmannJSONParser(size_t contentSize)
          : m_Buf(contentSize + 1), m_Offset(0)
      {
        assert(contentSize != 0);
      }

      std::vector< char > m_Buf;
      size_t m_Offset;

      bool
      FeedData(gsl::span< const char > buffer) override
      {
        if(m_Offset + buffer.size() > m_Buf.size() - 1)
        {
          return false;
        }
        std::copy(buffer.begin(), buffer.end(), m_Buf.begin());
        m_Offset += buffer.size();
        m_Buf[m_Offset] = 0;
        return true;
      }

      Result
      Parse(nlohmann::json& obj) const override
      {
        if(m_Offset < m_Buf.size() - 1)
        {
          return eNeedData;
        }

        try
        {
          obj = nlohmann::json::parse(m_Buf.data());
          return eDone;
        }
        catch(const nlohmann::json::exception&)
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
