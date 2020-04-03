#include "network_loki_lokinet_LokinetConfig.h"
#include <llarp.hpp>
#include "lokinet_jni_common.hpp"

extern "C"
{
  JNIEXPORT jobject JNICALL
  Java_network_loki_lokinet_LokinetConfig_Obtain(JNIEnv* env, jclass)
  {
    llarp_config* conf = llarp_default_config();
    if (conf == nullptr)
      return nullptr;
    return env->NewDirectByteBuffer(conf, llarp_config_size());
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_LokinetConfig_Free(JNIEnv* env, jclass, jobject buf)
  {
    llarp_config_free(FromBuffer<llarp_config>(env, buf));
  }

  JNIEXPORT jboolean JNICALL
  Java_network_loki_lokinet_LokinetConfig_Load(JNIEnv* env, jobject self, jstring fname)
  {
    llarp_config* conf = GetImpl<llarp_config>(env, self);
    if (conf == nullptr)
      return JNI_FALSE;
    return VisitStringAsStringView<jboolean>(
        env, fname, [conf](llarp::string_view val) -> jboolean {
          const auto filename = llarp::string_view_string(val);
          if (llarp_config_read_file(conf, filename.c_str()))
            return JNI_TRUE;
          return JNI_FALSE;
        });
  }
}