#include "network_loki_lokinet_LokinetDaemon.h"
#include "lokinet_jni_common.hpp"
#include "lokinet_jni_vpnio.hpp"
#include <llarp.hpp>

extern "C"
{
  JNIEXPORT jobject JNICALL
  Java_network_loki_lokinet_LokinetDaemon_Obtain(JNIEnv* env, jclass)
  {
    auto* ptr = new llarp::Context();
    if (ptr == nullptr)
      return nullptr;
    return env->NewDirectByteBuffer(ptr, sizeof(llarp::Context));
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_LokinetDaemon_Free(JNIEnv* env, jclass, jobject buf)
  {
    auto ptr = FromBuffer<llarp::Context>(env, buf);
    delete ptr;
  }

  JNIEXPORT jboolean JNICALL
  Java_network_loki_lokinet_LokinetDaemon_Configure(JNIEnv* env, jobject self, jobject conf)
  {
    auto ptr = GetImpl<llarp::Context>(env, self);
    auto config = GetImpl<llarp::Config>(env, conf);
    if (ptr == nullptr || config == nullptr)
      return JNI_FALSE;
    try
    {
      llarp::RuntimeOptions opts{};
      ptr->Configure(*config);
      ptr->Setup(opts);
    }
    catch (...)
    {
      return JNI_FALSE;
    }
    return JNI_TRUE;
  }

  JNIEXPORT jint JNICALL
  Java_network_loki_lokinet_LokinetDaemon_Mainloop(JNIEnv* env, jobject self)
  {
    auto ptr = GetImpl<llarp::Context>(env, self);
    if (ptr == nullptr)
      return -1;
    llarp::RuntimeOptions opts{};
    return ptr->Run(opts);
  }

  JNIEXPORT jboolean JNICALL
  Java_network_loki_lokinet_LokinetDaemon_IsRunning(JNIEnv* env, jobject self)
  {
    auto ptr = GetImpl<llarp::Context>(env, self);
    return (ptr != nullptr && ptr->IsUp()) ? JNI_TRUE : JNI_FALSE;
  }

  JNIEXPORT jboolean JNICALL
  Java_network_loki_lokinet_LokinetDaemon_Stop(JNIEnv* env, jobject self)
  {
    auto ptr = GetImpl<llarp::Context>(env, self);
    if (ptr == nullptr)
      return JNI_FALSE;
    if (not ptr->IsUp())
      return JNI_FALSE;
    ptr->CloseAsync();
    ptr->Wait();
    return ptr->IsUp() ? JNI_FALSE : JNI_TRUE;
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_LokinetDaemon_InjectVPNFD(JNIEnv* env, jobject self)
  {
    auto ptr = GetImpl<llarp::Context>(env, self);

    int fd = GetObjectMemberAsInt<int>(env, self, "m_FD");

    ptr->SetAndroidFD(fd);
  }
}
