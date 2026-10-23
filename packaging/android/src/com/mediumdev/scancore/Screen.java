package com.mediumdev.scancore;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.os.Build;
import android.view.OrientationEventListener;
import android.view.View;
import android.view.WindowManager;
import android.view.WindowInsetsController;

// Turning the phone for the anime player's full screen (src/app/Platform.cpp,
// WatchPage.qml), keeping the screen on, and the bars' icons.
public final class Screen {
    private Screen() {}

    private static OrientationEventListener release;

    // The screen stays on while a video plays (src/app/KeepAwake.cpp).
    public static void keepOn(final Activity activity, final boolean on) {
        activity.runOnUiThread(() -> {
            if (on) activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            else activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        });
    }

    // Status and navigation bar icons for the screen under them: light icons
    // on SCANCORE's dark screens, dark icons on light ones, whatever the
    // phone's own light/dark mode is (src/qml/Main.qml, Theme.dark).
    public static void barIcons(final Activity activity, final boolean darkScreen) {
        activity.runOnUiThread(() -> {
            View decor = activity.getWindow().getDecorView();
            if (Build.VERSION.SDK_INT >= 30) {
                WindowInsetsController c = decor.getWindowInsetsController();
                if (c == null) return;
                int mask = WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS
                        | WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
                c.setSystemBarsAppearance(darkScreen ? 0 : mask, mask);
            } else if (Build.VERSION.SDK_INT >= 23) {
                int flags = decor.getSystemUiVisibility();
                int light = View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
                if (Build.VERSION.SDK_INT >= 26) light |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                decor.setSystemUiVisibility(darkScreen ? flags & ~light : flags | light);
            }
        });
    }

    // "landscape": landscape whichever way the phone is held, even with
    // auto-rotate off. "portrait": portrait until the phone is held upright,
    // then free again, so turning it later still goes full screen. Anything
    // else: free (the system's rotation setting decides).
    public static void orient(final Activity activity, final String mode) {
        activity.runOnUiThread(() -> {
            if (release != null) {
                release.disable();
                release = null;
            }
            if ("landscape".equals(mode)) {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
                return;
            }
            if (!"portrait".equals(mode)) {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
                return;
            }
            activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            OrientationEventListener listener = new OrientationEventListener(activity) {
                @Override
                public void onOrientationChanged(int degrees) {
                    if (degrees == ORIENTATION_UNKNOWN || (degrees > 20 && degrees < 340)) return;
                    disable();
                    if (release == this) release = null;
                    activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
                }
            };
            if (listener.canDetectOrientation()) {
                release = listener;
                listener.enable();
            } else {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
            }
        });
    }
}
