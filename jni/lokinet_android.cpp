
//#include <string.h>
#include <jni.h>
#include <llarp.h>
#include <signal.h>
#include <thread>

struct AndroidMain 
{
  llarp_main * m_impl = nullptr;
  std::thread * m_thread = nullptr;

  ~AndroidMain()
  {
    if(m_impl)
      llarp_main_free(m_impl);
    if(m_thread)
    {
      m_thread->join();
      delete m_thread;
    }
  }

  void Start()
  {
    if(m_impl || m_thread)
      return;
    m_impl = llarp_main_init("daemon.ini", true);
    m_thread = new std::thread(std::bind(&AndroidMain::Run, this));
  }

  void Run()
  {
    llarp_main_run(m_impl);
  }

  void Stop()
  {
    llarp_main_signal(m_impl, SIGINT);
  }

  typedef std::unique_ptr<AndroidMain> Ptr;

  static std::atomic<Ptr> daemon = std::make_unique<AndroidMain>();
}

extern "C"
{

JNIEXPORT jstring JNICALL Java_network_loki_lokinet_Lokinet_1JNI_startLokinet
  (JNIEnv * env, jclass jcl)
  {
    if(AndroidMain::daemon->Running())
      return env->NewUTFString("already running");
    AndroidMain::daemon->Start();
    return env->NewUTFString("ok");
  }

}