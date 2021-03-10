#include "gotname.hpp"
#include <oxenmq/bt_serialize.h>
#include <llarp/dht/context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/path/path_context.hpp>

namespace llarp::dht
{
  constexpr size_t NameSizeLimit = 128;

  GotNameMessage::GotNameMessage(const Key_t& from, uint64_t txid, service::EncryptedName data)
      : IMessage(from), result(std::move(data)), TxID(txid)
  {
    if (result.ciphertext.size() > NameSizeLimit)
      throw std::invalid_argument("name data too big");
  }

  bool
  GotNameMessage::BEncode(llarp_buffer_t* buf) const
  {
    const std::string nonce((const char*)result.nonce.data(), result.nonce.size());
    const auto data = oxenmq::bt_serialize(
        oxenmq::bt_dict{{"A", "M"sv}, {"D", result.ciphertext}, {"N", nonce}, {"T", TxID}});
    return buf->write(data.begin(), data.end());
  }

  bool
  GotNameMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    if (key == "D")
    {
      llarp_buffer_t str{};
      if (not bencode_read_string(val, &str))
        return false;
      if (str.sz > NameSizeLimit)
        return false;
      result.ciphertext.resize(str.sz);
      std::copy_n(str.cur, str.sz, result.ciphertext.data());
      return true;
    }
    if (key == "N")
    {
      return result.nonce.BDecode(val);
    }
    if (key == "T")
    {
      return bencode_read_integer(val, &TxID);
    }
    return bencode_discard(val);
  }

  bool
  GotNameMessage::HandleMessage(struct llarp_dht_context* ctx, std::vector<Ptr_t>&) const
  {
    auto pathset = ctx->impl->GetRouter()->pathContext().GetLocalPathSet(pathID);
    if (pathset == nullptr)
      return false;
    auto copy = std::make_shared<const GotNameMessage>(*this);
    return pathset->HandleGotNameMessage(copy);
  }

}  // namespace llarp::dht
