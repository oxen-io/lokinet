
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
  std::string configFile;

  /// set configuration and ensure files
  bool 
  Configure(const char * conf, const char * basedir)
  {
    configFile = conf;
    return llarp_ensure_config(conf, basedir, false, false);
  }

  /// reload config on runtime
  bool
  ReloadConfig()
  {
    if(!m_Impl)
      return false;
    llarp_main_signal(m_impl, SIGHUP);
    return true;
  }

  /// start daemon thread
  bool
  Start()
  {
    if(m_impl || m_thread)
      return true;
    m_impl = llarp_main_init(configFile.c_str(), true);
    if(m_impl == nullptr)
      return false;
    m_thread = new std::thread(std::bind(&AndroidMain::Run, this));
    return true;
  }

  /// return true if we are running
  bool
  Running() const
  {
    return m_impl != nullptr && m_thread != nullptr;
  }

  /// blocking run
  void
  Run()
  {
    llarp_main_run(m_impl);
  }

  /// stop daemon thread
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
    if(daemon->Configure(conf.c_str(), basepath.string().c_str()))
    {
      if(daemon->Start())
        return env->NewStringUTF("ok");
      else 
        return env->NewStringUTF("failed to start daemon");
    }
    else
      return ev->NewStringUTF("failed to configure daemon");
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
                                                               jboolean isConnected)
  {
    if(isConnected)
    {
      if(!daemon->Running())
      {
        if(!daemon->Start())
        {
          // TODO: do some kind of callback here
        }
      }
    }
    else if(daemon->Running())
    {
      daemon->Stop();
    }
  }
}