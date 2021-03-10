#pragma once

#include <llarp.h>
#include <memory>
#include <future>
#include <string_view>
#include <algorithm>
#include <jni.h>

namespace lokinet
{
  struct VPNIO
  {
    static VPNIO*
    Get(llarp_vpn_io* vpn)
    {
      return static_cast<VPNIO*>(vpn->user);
    }

    virtual ~VPNIO() = default;

    llarp_vpn_io io;
    llarp_vpn_ifaddr_info info{{0}, {0}, 0};
    std::unique_ptr<std::promise<void>> closeWaiter;

    void
    Closed()
    {
      if (closeWaiter)
        closeWaiter->set_value();
    }

    virtual void
    InjectSuccess() = 0;

    virtual void
    InjectFail() = 0;

    virtual void
    Tick() = 0;

    VPNIO()
    {
      io.impl = nullptr;
      io.user = this;
      io.closed = [](llarp_vpn_io* vpn) { VPNIO::Get(vpn)->Closed(); };
      io.injected = [](llarp_vpn_io* vpn, bool good) {
        VPNIO* ptr = VPNIO::Get(vpn);
        if (good)
          ptr->InjectSuccess();
        else
          ptr->InjectFail();
      };
      io.tick = [](llarp_vpn_io* vpn) { VPNIO::Get(vpn)->Tick(); };
    }

    bool
    Init(llarp::Context* ptr)
    {
      if (Ready())
        return false;
      return llarp_vpn_io_init(ptr, &io);
    }

    bool
    Ready() const
    {
      return io.impl != nullptr;
    }

    void
    Close()
    {
      if (not Ready())
        return;
      if (closeWaiter)
        return;
      closeWaiter = std::make_unique<std::promise<void>>();
      llarp_vpn_io_close_async(&io);
      closeWaiter->get_future().wait();
      closeWaiter.reset();
      io.impl = nullptr;
    }

    llarp_vpn_pkt_reader*
    Reader()
    {
      return llarp_vpn_io_packet_reader(&io);
    }

    llarp_vpn_pkt_writer*
    Writer()
    {
      return llarp_vpn_io_packet_writer(&io);
    }

    ssize_t
    ReadPacket(void* dst, size_t len)
    {
      if (not Ready())
        return -1;
      unsigned char* buf = (unsigned char*)dst;
      return llarp_vpn_io_readpkt(Reader(), buf, len);
    }

    bool
    WritePacket(void* pkt, size_t len)
    {
      if (not Ready())
        return false;
      unsigned char* buf = (unsigned char*)pkt;
      return llarp_vpn_io_writepkt(Writer(), buf, len);
    }

    void
    SetIfName(std::string_view val)
    {
      const auto sz = std::min(val.size(), sizeof(info.ifname));
      std::copy_n(val.data(), sz, info.ifname);
    }

    void
    SetIfAddr(std::string_view val)
    {
      const auto sz = std::min(val.size(), sizeof(info.ifaddr));
      std::copy_n(val.data(), sz, info.ifaddr);
    }
  };
}  // namespace lokinet

struct lokinet_jni_vpnio : public lokinet::VPNIO
{
  void
  InjectSuccess() override
  {}

  void
  InjectFail() override
  {}

  void
  Tick() override
  {}
};
