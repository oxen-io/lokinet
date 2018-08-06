
//#include <string.h>
#include <jni.h>
#include <llarp.h>
#include <signal.h>
#include <memory>
#include <thread>

struct AndroidMain
{
  llarp_main* m_impl    = nullptr;
  std::thread* m_thread = nullptr;

  bool
  Start(const char* conf)
  {
    if(m_impl || m_thread)
      return true;
    printf("starting with config file %s", conf);
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
    {
      const char* nativeString = env->GetStringChars(configfile, JNI_TRUE);
      conf                     = nativeString;
      env->ReleaseStringChars(configfile, nativeString);
    }
    if(daemon->Start(conf.c_str()))
      return env->NewStringUTF("ok");
    else
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