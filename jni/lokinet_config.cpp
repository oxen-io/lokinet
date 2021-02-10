#include "network_loki_lokinet_LokinetConfig.h"
#include <llarp.hpp>
#include <config/config.hpp>
#include "lokinet_jni_common.hpp"

extern "C"
{
  JNIEXPORT jobject JNICALL
  Java_network_loki_lokinet_LokinetConfig_Obtain(JNIEnv* env, jclass, jstring dataDir)
  {
    auto conf = VisitStringAsStringView<llarp::Config*>(env, dataDir, [](std::string_view val) -> llarp::Config* {
      return new llarp::Config{val};
    });

    if (conf == nullptr)
      return nullptr;
    return env->NewDirectByteBuffer(conf, sizeof(llarp::Config));
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_LokinetConfig_Free(JNIEnv* env, jclass, jobject buf)
  {
    auto ptr = FromBuffer<llarp::Config>(env, buf);
    delete ptr;
  }

  JNIEXPORT jboolean JNICALL
  Java_network_loki_lokinet_LokinetConfig_Load(JNIEnv* env, jobject self)
  {
    auto conf = GetImpl<llarp::Config>(env, self);
    if (conf == nullptr)
      return JNI_FALSE;
    if (conf->Load(std::nullopt, false))
    {
      return JNI_TRUE;
    }
    return JNI_FALSE;
  }
}
