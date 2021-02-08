package network.loki.lokinet;

import java.lang.Thread;
import java.nio.ByteBuffer;
import java.io.File;

import android.net.VpnService;
import android.util.Log;
import android.content.Intent;
import android.os.ParcelFileDescriptor;

public class LokinetDaemon extends VpnService
{
  static {
    System.loadLibrary("lokinet-android");
  }

  public native ByteBuffer Obtain();
  public native void Free(ByteBuffer buf);
  public native boolean Configure(LokinetConfig config);
  public native int Mainloop();
  public native boolean IsRunning();
  public native boolean Stop();
  public native void InjectVPNFD();

  public static final String LOG_TAG = "LokinetDaemon";

  ByteBuffer impl = null;
  LokinetConfig config = new LokinetConfig();
  ParcelFileDescriptor iface;
  int m_FD = -1;

  @Override
    public void onCreate()
    {
      super.onCreate();
    }

  @Override
    public void onDestroy()
    {
      super.onDestroy();

      if (IsRunning())
      {
        Stop();
      }
      if (impl != null)
      {
        Free(impl);
      }
    }

  public int onStartCommand(Intent intent, int flags, int startID)
  {
    Log.d(LOG_TAG, "onStartCommand()");

    if (!IsRunning())
    {
      if (impl != null)
      {
        Free(impl);
      }
      impl = Obtain();
      if (impl == null)
      {
        Log.e(LOG_TAG, "got nullptr when creating llarp::Context in jni");
        return START_NOT_STICKY;
      }

      String dataDir = getFilesDir().toString();

      if (!config.LoadConfigFile(dataDir))
      {
        Log.e(LOG_TAG, "failed to load (or create) config file at: " + dataDir + "/lokinet.ini");
        return START_NOT_STICKY;
      }

      VpnService.Builder builder = new VpnService.Builder();

      builder.setMtu(1500);
      builder.addAddress("172.16.0.1", 16);
      builder.addRoute("0.0.0.0", 0);
      builder.addDnsServer("1.1.1.1"); // dummy address, native code will intercept dns
      builder.setSession("Lokinet");
      builder.setConfigureIntent(null);

      iface = builder.establish();
      if (iface == null)
      {
        Log.e(LOG_TAG, "VPN Interface from builder.establish() came back null");
        return START_NOT_STICKY;
      }

      m_FD = iface.detachFd();

      InjectVPNFD();

      if (!Configure(config))
      {
        //TODO: close vpn FD if this fails, either on native side, or here if possible
        Log.e(LOG_TAG, "failed to configure daemon");
        return START_NOT_STICKY;
      }

      new Thread(() -> {
          Mainloop();
          }).start();

      Log.d(LOG_TAG, "started successfully!");
    }
    else
    {
      Log.d(LOG_TAG, "already running");
    }

    return START_STICKY;
  }
}
