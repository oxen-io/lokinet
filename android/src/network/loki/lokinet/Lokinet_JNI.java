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

    /** get interface address we want */
    public static native String getIfAddr();

    /** get interface address range we want */
    public static native int getIfRange();

    /**
     * change network status
     */
	public static native void onNetworkStateChanged(boolean isConnected);

    /**
     * set vpn network interface fd
     * @param fd the file descriptor of the vpn interface
     */
    public static native void setVPNFileDescriptor(int fd);

    /**
     * load jni libraries
     */
	public static void loadLibraries() {
        System.loadLibrary("lokinetandroid");
    }
}
