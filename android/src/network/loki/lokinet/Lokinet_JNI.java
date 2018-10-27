package network.loki.lokinet;

public class Lokinet_JNI {

    public static final String STATUS_OK = "ok";

    public static native String getABICompiledWith();

	/**
	 * returns error info if failed
	 * returns "ok" if daemon initialized and started okay
	 */
    public static native String startLokinet(String config);
    
    /**
     * stop daemon if running
     */
    public static native void stopLokinet();

    /**
     * change network status
     */
	public static native void onNetworkStateChanged(boolean isConnected);

    /**
     * load jni libraries
     */
	public static void loadLibraries() {
        System.loadLibrary("lokinetandroid");
    }
}
