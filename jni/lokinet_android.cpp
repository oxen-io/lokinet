
//#include <string.h>
#include <jni.h>
#include <llarp.h>
#include <llarp/config.h>
#include <signal.h>
#include <memory>
#include <thread>
#include "fs.hpp"

struct AndroidMain
{
  llarp_main* m_impl    = nullptr;
  std::thread* m_thread = nullptr;

  bool
  Start(const char* conf, const char* basedir)
  {
    if(m_impl || m_thread)
      return true;
    if(!llarp_ensure_config(conf, basedir, false, false))
      return false;
    m_impl = llarp_main_init(conf, true);
    if(m_impl == nullptr)
      return false;
    m_thread = new std::thread(std::bind(&AndroidMain::Run, this));
    return true;
  }

  bool
  Running() const
  {
    return m_impl != nullptr;
  }

  void
  Run()
  {
    printf("running\n");
    llarp_main_run(m_impl);
  }

  void
  Stop()
  {
    llarp_main_signal(m_impl, SIGINT);
    m_thread->join();
    delete m_thread;
    m_thread = nullptr;
    llarp_main_free(m_impl);
    m_impl = nullptr;
  }

  typedef std::unique_ptr< AndroidMain > Ptr;
};

static AndroidMain::Ptr daemon(new AndroidMain());

extern "C"
{
  JNIEXPORT jstring JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_getABICompiledWith(JNIEnv* env, jclass)
  {
    // TODO: fixme
    return env->NewStringUTF("android");
  }

  JNIEXPORT jstring JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_startLokinet(JNIEnv* env, jclass jcl,
                                                      jstring configfile)
  {
    if(daemon->Running())
      return env->NewStringUTF("already running");
    std::string conf;
    fs::path basepath;
    {
      const char* nativeString = env->GetStringUTFChars(configfile, JNI_FALSE);
      conf += std::string(nativeString);
      env->ReleaseStringUTFChars(configfile, nativeString);
      basepath = fs::path(conf).parent_path();
    }
    if(daemon->Start(conf.c_str(), basepath.string().c_str()))
      return env->NewStringUTF("ok");
    return env->NewStringUTF("failed to start");
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_stopLokinet(JNIEnv* env, jclass)
  {
    if(daemon->Running())
    {
      daemon->Stop();
    }
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_onNetworkStateChanged(JNIEnv*, jclass,
                                                               jboolean)
  {
  }
}