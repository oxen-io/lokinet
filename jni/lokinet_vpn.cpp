#include "network_loki_lokinet_LokinetVPN.h"
#include "lokinet_jni_vpnio.hpp"
#include "lokinet_jni_common.hpp"
#include <net/ip.hpp>

extern "C"
{
  JNIEXPORT jint JNICALL
  Java_network_loki_lokinet_LokinetVPN_PacketSize(JNIEnv*, jclass)
  {
    return llarp::net::IPPacket::MaxSize;
  }

  JNIEXPORT jobject JNICALL
  Java_network_loki_lokinet_LokinetVPN_Alloc(JNIEnv* env, jclass)
  {
    lokinet_jni_vpnio* vpn = new lokinet_jni_vpnio();
    return env->NewDirectByteBuffer(vpn, sizeof(lokinet_jni_vpnio));
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_LokinetVPN_Free(JNIEnv* env, jclass, jobject buf)
  {
    lokinet_jni_vpnio* vpn = FromBuffer<lokinet_jni_vpnio>(env, buf);
    if (vpn == nullptr)
      return;
    delete vpn;
  }
  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_LokinetVPN_Stop(JNIEnv* env, jobject self)
  {
    lokinet_jni_vpnio* vpn = GetImpl<lokinet_jni_vpnio>(env, self);
    if (vpn)
    {
      vpn->Close();
    }
  }

  JNIEXPORT jint JNICALL
  Java_network_loki_lokinet_LokinetVPN_ReadPkt(JNIEnv* env, jobject self, jobject pkt)
  {
    lokinet_jni_vpnio* vpn = GetImpl<lokinet_jni_vpnio>(env, self);
    if (vpn == nullptr)
      return -1;
    void* pktbuf = env->GetDirectBufferAddress(pkt);
    auto pktlen = env->GetDirectBufferCapacity(pkt);
    if (pktbuf == nullptr)
      return -1;
    return vpn->ReadPacket(pktbuf, pktlen);
  }

  JNIEXPORT jboolean JNICALL
  Java_network_loki_lokinet_LokinetVPN_WritePkt(JNIEnv* env, jobject self, jobject pkt)
  {
    lokinet_jni_vpnio* vpn = GetImpl<lokinet_jni_vpnio>(env, self);
    if (vpn == nullptr)
      return false;
    void* pktbuf = env->GetDirectBufferAddress(pkt);
    auto pktlen = env->GetDirectBufferCapacity(pkt);
    if (pktbuf == nullptr)
      return false;
    return vpn->WritePacket(pktbuf, pktlen);
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_LokinetVPN_SetInfo(JNIEnv* env, jobject self, jobject info)
  {
    lokinet_jni_vpnio* vpn = GetImpl<lokinet_jni_vpnio>(env, self);
    if (vpn == nullptr)
      return;
    VisitObjectMemberStringAsStringView<bool>(
        env, info, "ifaddr", [vpn](llarp::string_view val) -> bool {
          vpn->SetIfAddr(val);
          return true;
        });
    VisitObjectMemberStringAsStringView<bool>(
        env, info, "ifname", [vpn](llarp::string_view val) -> bool {
          vpn->SetIfName(val);
          return true;
        });
    vpn->info.netmask = GetObjectMemberAsInt<uint8_t>(env, info, "netmask");
  }
}