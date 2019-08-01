#include <utp/linklayer.hpp>

#include <utp/session.hpp>

#ifdef __linux__
#include <linux/errqueue.h>
#include <netinet/ip_icmp.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#endif

#ifndef IP_DONTFRAGMENT
#define IP_DONTFRAGMENT IP_DONTFRAG
#endif

#include <functional>
#include <string.h>

namespace llarp
{
  namespace utp
  {
    uint64
    LinkLayer::OnConnect(utp_callback_arguments* arg)
    {
      LinkLayer* l =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
      Session* session = static_cast< Session* >(utp_get_userdata(arg->socket));
      if(session && l)
        session->OutboundLinkEstablished(l);
      else
        utp_close(arg->socket);
      return 0;
    }

    uint64
    LinkLayer::SendTo(utp_callback_arguments* arg)
    {
      LinkLayer* l =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
      if(l == nullptr)
        return 0;
      LogDebug("utp_sendto ", Addr(*arg->address), " ", arg->len, " bytes");
      return l->m_udp.sendto(&l->m_udp, arg->address, arg->buf, arg->len);
    }

    uint64
    LinkLayer::OnError(utp_callback_arguments* arg)
    {
      Session* session = static_cast< Session* >(utp_get_userdata(arg->socket));

      LinkLayer* link =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));

      if(session && link)
      {
        link->HandleTimeout(session);
        session->Close();
      }
      else
      {
        utp_close(arg->socket);
      }
      return 0;
    }

    uint64
    LinkLayer::OnLog(utp_callback_arguments* arg)
    {
      LogDebug(arg->buf);
      return 0;
    }

    LinkLayer::LinkLayer(const SecretKey& routerEncSecret, GetRCFunc getrc,
                         LinkMessageHandler h, SignBufferFunc sign,
                         SessionEstablishedHandler established,
                         SessionRenegotiateHandler reneg,
                         TimeoutHandler timeout, SessionClosedHandler closed,
                         bool permitInbound)
        : ILinkLayer(routerEncSecret, getrc, h, sign, established, reneg,
                     timeout, closed)
    {
      _utp_ctx = utp_init(2);
      utp_context_set_userdata(_utp_ctx, this);
      utp_set_callback(_utp_ctx, UTP_SENDTO, &LinkLayer::SendTo);
      if(permitInbound)
        utp_set_callback(_utp_ctx, UTP_ON_ACCEPT, &LinkLayer::OnAccept);
      utp_set_callback(_utp_ctx, UTP_ON_CONNECT, &LinkLayer::OnConnect);
      utp_set_callback(_utp_ctx, UTP_ON_STATE_CHANGE,
                       &LinkLayer::OnStateChange);
      utp_set_callback(_utp_ctx, UTP_ON_READ, &LinkLayer::OnRead);
      utp_set_callback(_utp_ctx, UTP_ON_ERROR, &LinkLayer::OnError);
      utp_set_callback(_utp_ctx, UTP_LOG, &LinkLayer::OnLog);
      utp_context_set_option(_utp_ctx, UTP_LOG_NORMAL, 1);
      utp_context_set_option(_utp_ctx, UTP_LOG_MTU, 1);
      utp_context_set_option(_utp_ctx, UTP_LOG_DEBUG, 1);
      utp_context_set_option(
          _utp_ctx, UTP_SNDBUF,
          (MAX_LINK_MSG_SIZE * MaxSendQueueSize * size_t{3}) / size_t{2});
      utp_context_set_option(
          _utp_ctx, UTP_RCVBUF,
          (MAX_LINK_MSG_SIZE * MaxSendQueueSize * size_t{3}) / size_t{2});
    }

    LinkLayer::~LinkLayer()
    {
      m_Pending.clear();
      m_AuthedLinks.clear();
      utp_destroy(_utp_ctx);
    }

    uint16_t
    LinkLayer::Rank() const
    {
      return 1;
    }

    void
    LinkLayer::RecvFrom(const Addr& from, const void* buf, size_t sz)
    {
      utp_process_udp(_utp_ctx, (const byte_t*)buf, sz, from, from.SockLen());
    }

#ifdef __linux__

    void
    LinkLayer::ProcessICMP()
    {
#ifndef TESTNET
      do
      {
        byte_t vec_buf[4096], ancillary_buf[4096];
        struct iovec iov = {vec_buf, sizeof(vec_buf)};
        struct sockaddr_in remote;
        struct msghdr msg;
        ssize_t len;
        struct cmsghdr* cmsg;
        struct sock_extended_err* e;
        struct sockaddr* icmp_addr;
        struct sockaddr_in* icmp_sin;

        memset(&msg, 0, sizeof(msg));

        msg.msg_name       = &remote;
        msg.msg_namelen    = sizeof(remote);
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;
        msg.msg_flags      = 0;
        msg.msg_control    = ancillary_buf;
        msg.msg_controllen = sizeof(ancillary_buf);

        len = recvmsg(m_udp.fd, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
        if(len < 0)
        {
          if(errno == EAGAIN || errno == EWOULDBLOCK)
            errno = 0;
          else
            LogError("failed to read icmp for utp ", strerror(errno));
          return;
        }

        for(cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
          if(cmsg->cmsg_type != IP_RECVERR)
          {
            continue;
          }
          if(cmsg->cmsg_level != SOL_IP)
          {
            continue;
          }
          e = (struct sock_extended_err*)CMSG_DATA(cmsg);
          if(!e)
            continue;
          if(e->ee_origin != SO_EE_ORIGIN_ICMP)
          {
            continue;
          }
          icmp_addr = (struct sockaddr*)SO_EE_OFFENDER(e);
          icmp_sin  = (struct sockaddr_in*)icmp_addr;
          if(icmp_sin->sin_port != 0)
          {
            continue;
          }
          if(e->ee_type == 3 && e->ee_code == 4)
          {
            utp_process_icmp_fragmentation(_utp_ctx, vec_buf, len,
                                           (struct sockaddr*)&remote,
                                           sizeof(remote), e->ee_info);
          }
          else
          {
            utp_process_icmp_error(_utp_ctx, vec_buf, len,
                                   (struct sockaddr*)&remote, sizeof(remote));
          }
        }
      } while(true);
#endif
    }
#endif

    void
    LinkLayer::Pump()
    {
#ifdef __linux__
      ProcessICMP();
#endif
      utp_issue_deferred_acks(_utp_ctx);
      std::set< RouterID > sessions;
      {
        Lock l(&m_AuthedLinksMutex);
        auto itr = m_AuthedLinks.begin();
        while(itr != m_AuthedLinks.end())
        {
          sessions.insert(itr->first);
          ++itr;
        }
      }
      ILinkLayer::Pump();
      {
        Lock l(&m_AuthedLinksMutex);
        for(const auto& pk : sessions)
        {
          if(m_AuthedLinks.count(pk) == 0)
          {
            // all sessions were removed
            SessionClosed(pk);
          }
        }
      }
    }

    void
    LinkLayer::Stop()
    {
      ForEachSession([](ILinkSession* s) { s->Close(); });
    }

    bool
    LinkLayer::KeyGen(SecretKey& k)
    {
      CryptoManager::instance()->encryption_keygen(k);
      return true;
    }

    void
    LinkLayer::Tick(llarp_time_t now)
    {
      utp_check_timeouts(_utp_ctx);
      ILinkLayer::Tick(now);
    }

    utp_socket*
    LinkLayer::NewSocket()
    {
      return utp_create_socket(_utp_ctx);
    }

    const char*
    LinkLayer::Name() const
    {
      return "utp";
    }

    std::shared_ptr< ILinkSession >
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& addr)
    {
      return std::make_shared< OutboundSession >(this, NewSocket(), rc, addr);
    }

    uint64
    LinkLayer::OnRead(utp_callback_arguments* arg)
    {
      Session* self = static_cast< Session* >(utp_get_userdata(arg->socket));

      if(self)
      {
        if(self->state == Session::eClose)
        {
          return 0;
        }
        if(!self->Recv(arg->buf, arg->len))
        {
          LogDebug("recv fail for ", self->remoteAddr);
          self->Close();
          return 0;
        }
        utp_read_drained(arg->socket);
      }
      else
      {
        LogWarn("utp_socket got data with no underlying session");
        utp_close(arg->socket);
      }
      return 0;
    }

    uint64
    LinkLayer::OnStateChange(utp_callback_arguments* arg)
    {
      Session* session = static_cast< Session* >(utp_get_userdata(arg->socket));
      if(session)
      {
        if(arg->state == UTP_STATE_WRITABLE)
        {
          session->PumpWrite();
        }
        else if(arg->state == UTP_STATE_EOF)
        {
          LogDebug("got eof from ", session->remoteAddr);
          session->Close();
        }
      }
      return 0;
    }

    uint64
    LinkLayer::OnAccept(utp_callback_arguments* arg)
    {
      LinkLayer* self =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
      Addr remote(*arg->address);

      std::shared_ptr< ILinkSession > session =
          std::make_shared< InboundSession >(self, arg->socket, remote);
      if(!self->PutSession(session))
      {
        LogWarn("dropping inbound utp session from ", remote);
        // close later
        self->m_Logic->call_later(50, [=]() {
          session->Close();
        });
      }
      else
      {
        LogDebug("utp accepted from ", remote);
        session->OnLinkEstablished(self);
      }
      return 0;
    }

  }  // namespace utp

}  // namespace llarp
