package com.mediumdev.scancore;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.Window;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Proxy;

// Volume keys turn pages in the comic reader (src/app/Platform.cpp). Qt
// leaves them to the system unless told otherwise for the whole app, which
// would take them from the video player too, so the window's key dispatch is
// wrapped once and takes them only while the reader asks for them.
public final class VolumeKeys {
    private VolumeKeys() {}

    static native void nativeKey(boolean down);

    private static volatile boolean capture;
    private static boolean installed;

    public static void setCapture(final Activity activity, final boolean on) {
        capture = on;
        if (!on) return;
        activity.runOnUiThread(() -> {
            if (installed) return;
            installed = true;
            final Window window = activity.getWindow();
            final Window.Callback inner = window.getCallback();
            window.setCallback((Window.Callback) Proxy.newProxyInstance(
                Window.Callback.class.getClassLoader(), new Class<?>[] {Window.Callback.class},
                (proxy, method, args) -> {
                    if (capture && "dispatchKeyEvent".equals(method.getName())) {
                        KeyEvent event = (KeyEvent) args[0];
                        int code = event.getKeyCode();
                        if (code == KeyEvent.KEYCODE_VOLUME_DOWN || code == KeyEvent.KEYCODE_VOLUME_UP) {
                            if (event.getAction() == KeyEvent.ACTION_DOWN)
                                nativeKey(code == KeyEvent.KEYCODE_VOLUME_DOWN);
                            return true;
                        }
                    }
                    try {
                        return method.invoke(inner, args);
                    } catch (InvocationTargetException e) {
                        throw e.getCause();
                    }
                }));
        });
    }
}
