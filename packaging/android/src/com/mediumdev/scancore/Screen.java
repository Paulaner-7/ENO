package com.mediumdev.scancore;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.media.AudioManager;
import android.provider.Settings;
import android.os.Build;
import android.view.OrientationEventListener;
import android.view.View;
import android.view.WindowManager;
import android.view.WindowInsetsController;

// Turning the phone for the anime player's full screen (src/app/Platform.cpp,
// WatchPage.qml), keeping the screen on, the bars' icons, and the player's
// brightness and volume swipes.
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

    // The window's brightness, 0 to 1, for the player's swipe; below 0 gives
    // it back to the system. Only this window: the phone's setting stays.
    public static void setBrightness(final Activity activity, final float level) {
        activity.runOnUiThread(() -> {
            WindowManager.LayoutParams params = activity.getWindow().getAttributes();
            params.screenBrightness = level < 0 ? WindowManager.LayoutParams.BRIGHTNESS_OVERRIDE_NONE : Math.max(0.01f, Math.min(1f, level));
            activity.getWindow().setAttributes(params);
        });
    }

    // The brightness the window shows at: its own, else the system's.
    public static float brightness(Activity activity) {
        float own = activity.getWindow().getAttributes().screenBrightness;
        if (own >= 0) return own;
        try {
            return Settings.System.getInt(activity.getContentResolver(), Settings.System.SCREEN_BRIGHTNESS) / 255f;
        } catch (Exception e) {
            return 0.5f;
        }
    }

    // Media volume, 0 to 1 (the phone's own, as its volume keys set it).
    public static float mediaVolume(Activity activity) {
        AudioManager audio = (AudioManager) activity.getSystemService(Context.AUDIO_SERVICE);
        int max = audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        return max > 0 ? audio.getStreamVolume(AudioManager.STREAM_MUSIC) / (float) max : 0;
    }

    public static void setMediaVolume(Activity activity, float level) {
        AudioManager audio = (AudioManager) activity.getSystemService(Context.AUDIO_SERVICE);
        int max = audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
        audio.setStreamVolume(AudioManager.STREAM_MUSIC, Math.round(Math.max(0f, Math.min(1f, level)) * max), 0);
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
