#include <consensus/table.hpp>
#include <crypto/crypto.hpp>

namespace llarp
{
  namespace consensus
  {
    ShortHash
    Table::CalculateHash() const
    {
      ShortHash h;
      const llarp_buffer_t buf(begin()->data(), size());
      CryptoManager::instance()->shorthash(h, buf);
      return h;
    }
  }  // namespace consensus
}  // namespace llarp
