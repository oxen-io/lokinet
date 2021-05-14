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

  private static native ByteBuffer Obtain();
  private static native void Free(ByteBuffer buf);
  public native boolean Configure(LokinetConfig config);
  public native int Mainloop();
  public native boolean IsRunning();
  public native boolean Stop();
  public native void InjectVPNFD();
  public native int GetUDPSocket();

  private static native String DetectFreeRange();

  public static final String LOG_TAG = "LokinetDaemon";

  ByteBuffer impl = null;
  ParcelFileDescriptor iface;
  int m_FD = -1;
  int m_UDPSocket = -1;

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
        impl = null;
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
        impl = null;
      }
      impl = Obtain();
      if (impl == null)
      {
        Log.e(LOG_TAG, "got nullptr when creating llarp::Context in jni");
        return START_NOT_STICKY;
      }

      String dataDir = getFilesDir().toString();
      LokinetConfig config;
      try
      {
        config = new LokinetConfig(dataDir);
      }
      catch(RuntimeException ex)
      {
        Log.e(LOG_TAG, ex.toString());
        return START_NOT_STICKY;
      }

      // FIXME: make these configurable
      String exitNode = "exit.loki";
      String upstreamDNS = "1.1.1.1";
      String ourRange = DetectFreeRange();

      if(ourRange.isEmpty())
      {
        Log.e(LOG_TAG, "cannot detect free range");
        return START_NOT_STICKY;
      }


      // set up config values
      config.AddDefaultValue("network", "exit-node", exitNode);
      config.AddDefaultValue("network", "ifaddr", ourRange);
      config.AddDefaultValue("dns", "upstream", upstreamDNS);


      if (!config.Load())
      {
        Log.e(LOG_TAG, "failed to load (or create) config file at: " + dataDir + "/lokinet.ini");
        return START_NOT_STICKY;
      }

      VpnService.Builder builder = new VpnService.Builder();

      builder.setMtu(1500);

      String[] parts = ourRange.split("/");
      String ourIPv4 = parts[0];
      int ourMask = Integer.parseInt(parts[1]);

      // set ip4
      builder.addAddress(ourIPv4, ourMask);
      builder.addRoute("0.0.0.0", 0);
      // set ip6
      // TODO: convert ipv4 to fd00::/8 range for ipv6
      // builder.addAddress(ourIPv6, ourMask + 96);
      // builder.addRoute("::", 0);

      builder.addDnsServer(upstreamDNS);
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

      new Thread(() -> {
          Configure(config);
          m_UDPSocket = GetUDPSocket();
          protect(m_UDPSocket);
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
