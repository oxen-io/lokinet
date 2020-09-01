#include <dht/messages/gotname.hpp>
#include <lokimq/bt_serialize.h>

namespace llarp::dht
{
  constexpr size_t NameSizeLimit = 512;

  GotNameMessage::GotNameMessage(const Key_t& from, uint64_t txid, std::string data)
      : IMessage(from), Data(std::move(data)), TxID(txid)
  {
    if (Data.size() > NameSizeLimit)
      throw std::invalid_argument("name data too big");
  }

  bool
  GotNameMessage::BEncode(llarp_buffer_t* buf) const
  {
    const auto data = lokimq::bt_serialize(lokimq::bt_dict{{"A", "M"sv}, {"N", Data}, {"T", TxID}});
    return buf->write(data.begin(), data.end());
  }

  bool
  GotNameMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    if (key == "N")
    {
      llarp_buffer_t str{};
      if (not bencode_read_string(val, &str))
        return false;
      if (str.sz > NameSizeLimit)
        return false;
      Data.resize(str.sz);
      std::copy_n(str.cur, str.sz, Data.data());
      return true;
    }
    if (key == "T")
    {
      return bencode_read_integer(val, &TxID);
    }
    return bencode_discard(val);
  }

  bool
  GotNameMessage::HandleMessage(struct llarp_dht_context*, std::vector<Ptr_t>&) const
  {
    return false;
  }

}  // namespace llarp::dht
