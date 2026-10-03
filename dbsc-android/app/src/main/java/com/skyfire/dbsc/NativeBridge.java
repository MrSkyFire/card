package com.skyfire.dbsc;

public final class NativeBridge {
    static { System.loadLibrary("dbsc"); }
    private NativeBridge() {}
    public static native int runServer(String rootPath, int port);
    public static native void stopServer();
}
