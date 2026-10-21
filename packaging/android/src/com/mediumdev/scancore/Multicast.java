package com.mediumdev.scancore;

import android.content.Context;
import android.net.wifi.WifiManager;

// Android drops multicast packets unless an app holds this lock: held while
// looking for TVs and Chromecasts (a few seconds), released after.
public final class Multicast {
    private static WifiManager.MulticastLock lock;

    private Multicast() {}

    public static synchronized void hold(Context context, boolean on) {
        try {
            if (on) {
                if (lock == null) {
                    final WifiManager wifi = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
                    if (wifi == null)
                        return;
                    lock = wifi.createMulticastLock("scancore-cast");
                    lock.setReferenceCounted(false);
                }
                lock.acquire();
            } else if (lock != null && lock.isHeld()) {
                lock.release();
            }
        } catch (Exception e) {
            // Without it, only devices that answer directly are found.
        }
    }
}
