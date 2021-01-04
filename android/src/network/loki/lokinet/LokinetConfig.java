package network.loki.lokinet;

import java.nio.ByteBuffer;

public class LokinetConfig
{
  static {
    System.loadLibrary("lokinet-android");
  }

  public native ByteBuffer Obtain(String dataDir);
  public native void Free(ByteBuffer buf);
  public native boolean Load();

  ByteBuffer impl = null;

  LokinetConfig()
  {
  }

  public void finalize()
  {
    if (impl != null)
    {
      Free(impl);
    }
  }

  public boolean LoadConfigFile(String dataDir)
  {
    if (impl != null)
    {
      Free(impl);
    }

    impl = Obtain(dataDir);
    if (impl == null)
    {
      return false;
    }

    if (!Load())
    {
      Free(impl);
      return false;
    }

    return true;
  }
}
