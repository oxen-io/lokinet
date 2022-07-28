#include <winsock2.h>
#include <windows.h>
#include "windivert.hpp"
#include "dll.hpp"
#include "handle.hpp"
#include <llarp/util/thread/queue.hpp>
#include <llarp/util/logging.hpp>
#include <thread>
extern "C"
{
#include <windivert.h>
}
namespace L = llarp::log;

namespace llarp::win32
{
  namespace
  {
    auto cat = L::Cat("windivert");
  }

  using WD_Open_Func_t = decltype(&::WinDivertOpen);
  using WD_Close_Func_t = decltype(&::WinDivertClose);
  using WD_Shutdown_Func_t = decltype(&::WinDivertShutdown);
  using WD_Send_Func_t = decltype(&::WinDivertSend);
  using WD_Recv_Func_t = decltype(&::WinDivertRecv);
  using WD_IP4_Format_Func_t = decltype(&::WinDivertHelperFormatIPv4Address);
  using WD_IP6_Format_Func_t = decltype(&::WinDivertHelperFormatIPv6Address);

  struct WinDivertDLL : DLL
  {
    WD_Open_Func_t open;
    WD_Close_Func_t close;
    WD_Shutdown_Func_t shutdown;
    WD_Send_Func_t send;
    WD_Recv_Func_t recv;
    WD_IP4_Format_Func_t format_ip4;
    WD_IP6_Format_Func_t format_ip6;

    WinDivertDLL() : DLL{"WinDivert.dll"}
    {
      init("WinDivertOpen", open);
      init("WinDivertClose", close);
      init("WinDivertShutdown", shutdown);
      init("WinDivertSend", send);
      init("WinDivertRecv", recv);
      init("WinDivertHelperFormatIPv4Address", format_ip4);
      init("WinDivertHelperFormatIPv6Address", format_ip6);
      L::debug(cat, "loaded windivert functions");
    }

    virtual ~WinDivertDLL() = default;
  };

  struct WD_Packet
  {
    std::vector<byte_t> pkt;
    WINDIVERT_ADDRESS addr;
  };

  class WinDivert_IO : public llarp::vpn::I_Packet_IO
  {
    const std::shared_ptr<WinDivertDLL> m_WinDivert;
    std::function<void(void)> m_Wake;

    HANDLE m_Handle;
    std::thread m_Runner;
    thread::Queue<WD_Packet> m_RecvQueue;

   public:
    WinDivert_IO(
        std::shared_ptr<WinDivertDLL> api, std::string filter_spec, std::function<void(void)> wake)
        : m_WinDivert{api}, m_Wake{wake}, m_RecvQueue{recv_queue_size}
    {
      L::info(cat, "load windivert with filterspec: '{}'", filter_spec);

      m_Handle = m_WinDivert->open(filter_spec.c_str(), WINDIVERT_LAYER_NETWORK, 0, 0);
      if (auto err = GetLastError())
        throw win32::error{err, "cannot open windivert handle"};
    }

    ~WinDivert_IO()
    {
      m_WinDivert->close(m_Handle);
    }

    std::optional<WD_Packet>
    recv_packet() const
    {
      WINDIVERT_ADDRESS addr{};
      std::vector<byte_t> pkt;
      pkt.resize(1500);  // net::IPPacket::MaxSize
      UINT sz{};
      if (not m_WinDivert->recv(m_Handle, pkt.data(), pkt.size(), &sz, &addr))
      {
        auto err = GetLastError();
        if (err and err != ERROR_BROKEN_PIPE)
          throw win32::error{
              err, fmt::format("failed to receive packet from windivert (code={})", err)};
        else if (err)
          SetLastError(0);
        return std::nullopt;
      }
      L::info(cat, "got packet of size {}B", sz);
      pkt.resize(sz);
      return WD_Packet{std::move(pkt), std::move(addr)};
    }

    void
    send_packet(const WD_Packet& w_pkt) const
    {
      const auto& pkt = w_pkt.pkt;
      const auto* addr = &w_pkt.addr;
      L::info(cat, "send dns packet of size {}B", pkt.size());
      UINT sz{};
      if (m_WinDivert->send(m_Handle, pkt.data(), pkt.size(), &sz, addr))
        return;
      throw win32::error{"windivert send failed"};
    }

    virtual int
    PollFD() const
    {
      return -1;
    }

    virtual bool WritePacket(net::IPPacket) override
    {
      return false;
    }

    virtual net::IPPacket
    ReadNextPacket() override
    {
      auto w_pkt = recv_packet();
      if (not w_pkt)
        return net::IPPacket{};
      net::IPPacket pkt{std::move(w_pkt->pkt)};
      pkt.reply = [this, addr = std::move(w_pkt->addr)](auto pkt) {
        send_packet(WD_Packet{pkt.steal(), addr});
      };
      return pkt;
    }

    virtual void
    Start() override
    {
      L::info(cat, "starting windivert");
      if (m_Runner.joinable())
        throw std::runtime_error{"windivert thread is already running"};

      auto read_loop = [this]() {
        log::debug(cat, "windivert read loop start");
        while (true)
        {
          // in the read loop, read packets until they stop coming in
          // each packet is sent off
          if (auto maybe_pkt = recv_packet())
            m_RecvQueue.pushBack(std::move(*maybe_pkt));
          else  // leave loop on read fail
            break;
        }
        log::debug(cat, "windivert read loop end");
      };

      m_Runner = std::thread{std::move(read_loop)};
    }

    virtual void
    Stop() override
    {
      L::info(cat, "stopping windivert");
      m_WinDivert->shutdown(m_Handle, WINDIVERT_SHUTDOWN_BOTH);
      m_Runner.join();
    }
  };

  WinDivert_API::WinDivert_API() : m_Impl{std::make_shared<WinDivertDLL>()}
  {}

  std::string
  WinDivert_API::format_ip(uint32_t ip) const
  {
    std::array<char, 128> buf{};
    m_Impl->format_ip4(ip, buf.data(), buf.size());
    return buf.data();
  }

  std::shared_ptr<llarp::vpn::I_Packet_IO>
  WinDivert_API::make_intercepter(std::string filter_spec, std::function<void(void)> wake) const
  {
    return std::make_shared<WinDivert_IO>(m_Impl, filter_spec, wake);
  }
}  // namespace llarp::win32
