package network.loki.lokinet;

import java.nio.ByteBuffer;

public class LokinetConfig
{
  static {
    System.loadLibrary("lokinet-android");
  }

  private static native ByteBuffer Obtain(String dataDir);
  private static native void Free(ByteBuffer buf);

  /*** load config file from disk */
  public native boolean Load();
  /*** save chages to disk */
  public native boolean Save();

  
  /** override default config value before loading from config file */
  public native void AddDefaultValue(String section, String key, String value);
  
  private final ByteBuffer impl;

  public LokinetConfig(String dataDir)
  {
    impl = Obtain(dataDir);
    if(impl == null)
      throw new RuntimeException("cannot obtain config from "+dataDir);
  }

  public void finalize()
  {
    if (impl != null)
    {
      Free(impl);
    }
  }
}
