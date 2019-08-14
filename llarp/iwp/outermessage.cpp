#include <iwp/outermessage.hpp>
#include <memory>

namespace llarp
{
  namespace iwp
  {
    std::array< byte_t, 6 > OuterMessage::obtain_flow_id_magic =
        std::array< byte_t, 6 >{{'n', 'e', 't', 'i', 'd', '?'}};

    std::array< byte_t, 6 > OuterMessage::give_flow_id_magic =
        std::array< byte_t, 6 >{{'n', 'e', 't', 'i', 'd', '!'}};

    OuterMessage::OuterMessage()
    {
      Clear();
    }

    OuterMessage::~OuterMessage() = default;

    void
    OuterMessage::Clear()
    {
      command = 0;
      flow.Zero();
      netid.Zero();
      reject.fill(0);
      N.Zero();
      X.Zero();
      Xsize = 0;
      Zsig.Zero();
      Zhash.Zero();
      pubkey.Zero();
      magic.fill(0);
      uinteger = 0;
      A.reset();
    }

    void
    OuterMessage::CreateReject(const char* msg, llarp_time_t now,
                               const PubKey& pk)
    {
      Clear();
      std::copy_n(msg, std::min(strlen(msg), reject.size()), reject.begin());
      uinteger = now;
      pubkey   = pk;
    }

    bool
    OuterMessage::Encode(llarp_buffer_t* buf) const
    {
      if(buf->size_left() < 2)
        return false;
      *buf->cur = command;
      buf->cur++;
      *buf->cur = '=';
      buf->cur++;
      switch(command)
      {
        case eOCMD_ObtainFlowID:

        case eOCMD_GiveFlowID:
          if(!buf->write(reject.begin(), reject.end()))
            return false;
          if(!buf->write(give_flow_id_magic.begin(), give_flow_id_magic.end()))
            return false;
          if(!buf->write(flow.begin(), flow.end()))
            return false;
          if(!buf->write(pubkey.begin(), pubkey.end()))
            return false;
          return buf->write(Zsig.begin(), Zsig.end());
        default:
          return false;
      }
    }

    bool
    OuterMessage::Decode(llarp_buffer_t* buf)
    {
      static constexpr size_t header_size = 2;

      if(buf->size_left() < header_size)
        return false;
      command = *buf->cur;
      ++buf->cur;
      if(*buf->cur != '=')
        return false;
      ++buf->cur;
      switch(command)
      {
        case eOCMD_ObtainFlowID:
          if(!buf->read_into(magic.begin(), magic.end()))
            return false;
          if(!buf->read_into(netid.begin(), netid.end()))
            return false;
          if(!buf->read_uint64(uinteger))
            return false;
          if(!buf->read_into(pubkey.begin(), pubkey.end()))
            return false;
          if(buf->size_left() <= Zsig.size())
            return false;
          Xsize = buf->size_left() - Zsig.size();
          if(!buf->read_into(X.begin(), X.begin() + Xsize))
            return false;
          return buf->read_into(Zsig.begin(), Zsig.end());
        case eOCMD_GiveFlowID:
          if(!buf->read_into(magic.begin(), magic.end()))
            return false;
          if(!buf->read_into(flow.begin(), flow.end()))
            return false;
          if(!buf->read_into(pubkey.begin(), pubkey.end()))
            return false;
          buf->cur += buf->size_left() - Zsig.size();
          return buf->read_into(Zsig.begin(), Zsig.end());
        case eOCMD_Reject:
          if(!buf->read_into(reject.begin(), reject.end()))
            return false;
          if(!buf->read_uint64(uinteger))
            return false;
          if(!buf->read_into(pubkey.begin(), pubkey.end()))
            return false;
          buf->cur += buf->size_left() - Zsig.size();
          return buf->read_into(Zsig.begin(), Zsig.end());
        case eOCMD_SessionNegotiate:
          if(!buf->read_into(flow.begin(), flow.end()))
            return false;
          if(!buf->read_into(pubkey.begin(), pubkey.end()))
            return false;
          if(!buf->read_uint64(uinteger))
            return false;
          if(buf->size_left() == Zsig.size() + 32)
          {
            A = std::make_unique< AlignedBuffer< 32 > >();
            if(!buf->read_into(A->begin(), A->end()))
              return false;
          }
          return buf->read_into(Zsig.begin(), Zsig.end());
        case eOCMD_TransmitData:
          if(!buf->read_into(flow.begin(), flow.end()))
            return false;
          if(!buf->read_into(N.begin(), N.end()))
            return false;
          if(buf->size_left() <= Zhash.size())
            return false;
          Xsize = buf->size_left() - Zhash.size();
          if(!buf->read_into(X.begin(), X.begin() + Xsize))
            return false;
          return buf->read_into(Zhash.begin(), Zhash.end());
        default:
          return false;
      }
    }
  }  // namespace iwp

}  // namespace llarp
