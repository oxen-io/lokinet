
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

  ~AndroidMain()
  {
    if(m_thread)
    {
      m_thread->join();
      delete m_thread;
    }
  }

  void
  Start()
  {
    if(m_impl || m_thread)
      return;
    m_impl   = llarp_main_init("daemon.ini", true);
    m_thread = new std::thread(std::bind(&AndroidMain::Run, this));
  }

  void
  Run()
  {
    llarp_main_run(m_impl);
    llarp_main_free(m_impl);
  }

  void
  Stop()
  {
    llarp_main_signal(m_impl, SIGINT);
  }

  typedef std::unique_ptr< AndroidMain > Ptr;

  static std::atomic< Ptr > daemon = std::make_unique< AndroidMain >();
}

extern "C"
{
  JNIEXPORT jstring JNICALL
      Java_network_loki_lokinet_Lokinet_1JNI_getABICompiledWith(JNIEnv * env,
                                                                jclass)
  {
    // TODO: fixme
    return env->NewUTFString("android");
  }

  JNIEXPORT jstring JNICALL Java_network_loki_lokinet_Lokinet_1JNI_startLokinet(
      JNIEnv * env, jclass jcl)
  {
    if(AndroidMain::daemon->Running())
      return env->NewUTFString("already running");
    AndroidMain::daemon->Start();
    return env->NewUTFString("ok");
  }

  JNIEXPORT void JNICALL Java_network_loki_lokinet_Lokinet_1JNI_stopLokinet(
      JNIEnv * env, jclass)
  {
    if(AndroidMain::daemon->Running())
    {
      AndroidMain::daemon->Stop();
    }
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_onNetworkStateChanged(JNIEnv*, jclass,
                                                               jboolean)
  {
  }
}