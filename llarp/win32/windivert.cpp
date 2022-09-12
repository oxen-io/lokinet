#include <winsock2.h>
#include <windows.h>
#include "windivert.hpp"
#include "dll.hpp"
#include "handle.hpp"
#include <llarp/util/thread/queue.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/logging/buffer.hpp>
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
  namespace wd
  {
    namespace
    {
      decltype(::WinDivertOpen)* open = nullptr;
      decltype(::WinDivertClose)* close = nullptr;
      decltype(::WinDivertShutdown)* shutdown = nullptr;
      decltype(::WinDivertSend)* send = nullptr;
      decltype(::WinDivertRecv)* recv = nullptr;
      decltype(::WinDivertHelperFormatIPv4Address)* format_ip4 = nullptr;
      decltype(::WinDivertHelperFormatIPv6Address)* format_ip6 = nullptr;

      void
      Initialize()
      {
        if (wd::open)
          return;

        // clang-format off
      load_dll_functions(
          "WinDivert.dll",

          "WinDivertOpen",                    open,
          "WinDivertClose",                   close,
          "WinDivertShutdown",                shutdown,
          "WinDivertSend",                    send,
          "WinDivertRecv",                    recv,
          "WinDivertHelperFormatIPv4Address", format_ip4,
          "WinDivertHelperFormatIPv6Address", format_ip6);
        // clang-format on
      }
    }  // namespace

    struct Packet
    {
      std::vector<byte_t> pkt;
      WINDIVERT_ADDRESS addr;
    };

    class IO : public llarp::vpn::I_Packet_IO
    {
      std::function<void(void)> m_Wake;

      HANDLE m_Handle;
      std::thread m_Runner;
      thread::Queue<Packet> m_RecvQueue;
      // dns packet queue size
      static constexpr size_t recv_queue_size = 64;

     public:
      IO(std::string filter_spec, std::function<void(void)> wake)
          : m_Wake{wake}, m_RecvQueue{recv_queue_size}
      {
        wd::Initialize();
        L::info(cat, "load windivert with filterspec: '{}'", filter_spec);

        m_Handle = wd::open(filter_spec.c_str(), WINDIVERT_LAYER_NETWORK, 0, 0);
        if (auto err = GetLastError())
          throw win32::error{err, "cannot open windivert handle"};
      }

      ~IO()
      {
        wd::close(m_Handle);
      }

      std::optional<Packet>
      recv_packet() const
      {
        WINDIVERT_ADDRESS addr{};
        std::vector<byte_t> pkt;
        pkt.resize(1500);  // net::IPPacket::MaxSize
        UINT sz{};
        if (not wd::recv(m_Handle, pkt.data(), pkt.size(), &sz, &addr))
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
        return Packet{std::move(pkt), std::move(addr)};
      }

      void
      send_packet(const Packet& w_pkt) const
      {
        const auto& pkt = w_pkt.pkt;
        const auto* addr = &w_pkt.addr;
        L::info(cat, "send dns packet of size {}B", pkt.size());
        UINT sz{};
        if (wd::send(m_Handle, pkt.data(), pkt.size(), &sz, addr))
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
        auto w_pkt = m_RecvQueue.tryPopFront();
        if (not w_pkt)
          return net::IPPacket{};
        net::IPPacket pkt{std::move(w_pkt->pkt)};
        pkt.reply = [this, addr = std::move(w_pkt->addr)](auto pkt) {
          send_packet(Packet{pkt.steal(), addr});
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
          log::info(cat, "windivert read loop start");
          while (true)
          {
            // in the read loop, read packets until they stop coming in
            // each packet is sent off
            if (auto maybe_pkt = recv_packet())
            {
              m_RecvQueue.pushBack(std::move(*maybe_pkt));
              // wake up event loop
              m_Wake();
            }
            else  // leave loop on read fail
              break;
          }
          log::info(cat, "windivert read loop end");
        };

        m_Runner = std::thread{std::move(read_loop)};
      }

      virtual void
      Stop() override
      {
        L::info(cat, "stopping windivert");
        wd::shutdown(m_Handle, WINDIVERT_SHUTDOWN_BOTH);
        m_Runner.join();
      }
    };

  }  // namespace wd

  namespace WinDivert
  {
    std::string
    format_ip(uint32_t ip)
    {
      std::array<char, 128> buf;
      wd::format_ip4(ip, buf.data(), buf.size());
      return buf.data();
    }

    std::shared_ptr<llarp::vpn::I_Packet_IO>
    make_intercepter(std::string filter_spec, std::function<void(void)> wake)
    {
      return std::make_shared<wd::IO>(filter_spec, wake);
    }
  }  // namespace WinDivert

}  // namespace llarp::win32
