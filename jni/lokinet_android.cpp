#include <llarp.h>
#include <config/config.hpp>
#include <util/fs.hpp>

#include <jni.h>
#include <signal.h>
#include <memory>
#include <thread>

struct AndroidMain
{
  llarp_main* m_impl    = nullptr;
  std::thread* m_thread = nullptr;
  std::string configFile;

  /// set configuration and ensure files
  bool
  Configure(const char* conf, const char* basedir)
  {
    configFile = conf;
    return llarp_ensure_config(conf, basedir, false, false);
  }

  /// reload config on runtime
  bool
  ReloadConfig()
  {
    if(!m_impl)
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
    if(llarp_main_setup(m_impl, false))
    {
      llarp_main_free(m_impl);
      m_impl = nullptr;
      return false;
    }
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
    if(llarp_main_run(m_impl))
    {
      // on error
      llarp::LogError("daemon run fail");
      llarp_main* ptr = m_impl;
      m_impl          = nullptr;
      llarp_main_signal(ptr, SIGINT);
      llarp_main_free(ptr);
    }
  }

  const char*
  GetIfAddr()
  {
    if(m_impl)
    {
      auto tun = main_router_getFirstTunEndpoint(m_impl);
      if(tun)
        return tun->tunif.ifaddr;
    }
    return "";
  }

  int
  GetIfRange() const
  {
    if(m_impl)
    {
      auto tun = main_router_getFirstTunEndpoint(m_impl);
      if(tun)
        return tun->tunif.netmask;
    }
    return -1;
  }

  void
  SetVPN_FD(int rfd, int wfd)
  {
    if(m_impl)
      llarp_main_inject_vpn_fd(m_impl, rfd, wfd);
  }

  /// stop daemon thread
  void
  Stop()
  {
    if(m_impl)
      llarp_main_signal(m_impl, SIGINT);
    m_thread->join();
    delete m_thread;
    m_thread = nullptr;
    if(m_impl)
      llarp_main_free(m_impl);
    m_impl = nullptr;
  }

  typedef std::unique_ptr< AndroidMain > Ptr;
};

static AndroidMain::Ptr daemon_ptr(new AndroidMain());

extern "C"
{
  JNIEXPORT jstring JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_getABICompiledWith(JNIEnv* env, jclass)
  {
    // TODO: fixme
    return env->NewStringUTF("android");
  }

  JNIEXPORT jstring JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_startLokinet(JNIEnv* env, jclass,
                                                      jstring configfile)
  {
    if(daemon_ptr->Running())
      return env->NewStringUTF("already running");
    std::string conf;
    fs::path basepath;
    {
      const char* nativeString = env->GetStringUTFChars(configfile, JNI_FALSE);
      conf += std::string(nativeString);
      env->ReleaseStringUTFChars(configfile, nativeString);
      basepath = fs::path(conf).parent_path();
    }
    if(daemon_ptr->Configure(conf.c_str(), basepath.string().c_str()))
    {
      if(daemon_ptr->Start())
        return env->NewStringUTF("ok");
      else
        return env->NewStringUTF("failed to start daemon");
    }
    else
      return env->NewStringUTF("failed to configure daemon");
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_stopLokinet(JNIEnv*, jclass)
  {
    if(daemon_ptr->Running())
    {
      daemon_ptr->Stop();
    }
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_setVPNFileDescriptor(JNIEnv*, jclass,
                                                              jint rfd,
                                                              jint wfd)
  {
    daemon_ptr->SetVPN_FD(rfd, wfd);
  }

  JNIEXPORT jstring JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_getIfAddr(JNIEnv* env, jclass)
  {
    if(daemon_ptr)
      return env->NewStringUTF(daemon_ptr->GetIfAddr());
    else
      return env->NewStringUTF("");
  }

  JNIEXPORT jint JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_getIfRange(JNIEnv*, jclass)
  {
    if(daemon_ptr)
      return daemon_ptr->GetIfRange();
    else
      return -1;
  }

  JNIEXPORT void JNICALL
  Java_network_loki_lokinet_Lokinet_1JNI_onNetworkStateChanged(
      JNIEnv*, jclass, jboolean isConnected)
  {
    if(isConnected)
    {
      if(!daemon_ptr->Running())
      {
        if(!daemon_ptr->Start())
        {
          // TODO: do some kind of callback here
        }
      }
    }
    else if(daemon_ptr->Running())
    {
      daemon_ptr->Stop();
    }
  }
}
