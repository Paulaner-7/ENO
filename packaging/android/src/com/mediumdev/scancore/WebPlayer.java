package com.mediumdev.scancore;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.webkit.GeolocationPermissions;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.widget.FrameLayout;

// The anime player's web view (src/qml/components/WebPlayerSystem.qml), which
// Qt WebView creates with two of Android's defaults that do not suit a player:
// - a page may only start video by itself muted, so every episode started
//   without sound;
// - pages are told full screen is not supported unless the web view's client
//   shows the page's full screen view. That view now covers the window, in
//   landscape and without the system bars, until the player leaves full
//   screen or Back is pressed.
public final class WebPlayer {
    private WebPlayer() {}

    // The player page may load: its web view is prepared, or none turned up.
    static native void nativePrepared();

    // Prepares the web view a player page has just created. Qt adds it to the
    // window a moment later, so this looks again for a few seconds. The page
    // waits for nativePrepared(): a player that starts before this finds no
    // sound and no full screen, and does not look again.
    public static void prepare(final Activity activity) {
        activity.runOnUiThread(() -> attempt(activity, 0));
    }

    // Every 100 ms; after 5 s the page loads anyway, and the search goes on
    // for half a minute in case the web view still turns up.
    private static void attempt(final Activity activity, final int tries) {
        if (activity.isFinishing() || activity.isDestroyed()) return;
        if (prepareIn(activity, activity.getWindow().getDecorView()) > 0) {
            if (tries <= 50) nativePrepared();
            return;
        }
        if (tries == 50) nativePrepared();
        if (tries < 300)
            new Handler(Looper.getMainLooper()).postDelayed(() -> attempt(activity, tries + 1), 100);
    }

    // How many web views were prepared now (not before).
    private static int prepareIn(Activity activity, View view) {
        if (view instanceof WebView) {
            WebView web = (WebView) view;
            WebChromeClient client = web.getWebChromeClient();
            if (client instanceof Client) return 0;
            web.getSettings().setMediaPlaybackRequiresUserGesture(false);
            web.setWebChromeClient(new Client(activity, web, client));
            return 1;
        }
        int prepared = 0;
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++)
                prepared += prepareIn(activity, group.getChildAt(i));
        }
        return prepared;
    }

    // Qt's own client (title, progress and icon reach QML through it) with
    // full screen added.
    private static final class Client extends WebChromeClient {
        private final Activity activity;
        private final WebView web;
        private final WebChromeClient qt;
        private FrameLayout cover;
        private CustomViewCallback callback;
        private int orientation;
        private boolean barsShown;
        // The player page closed while in full screen.
        private final View.OnAttachStateChangeListener closed = new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {}

            @Override
            public void onViewDetachedFromWindow(View view) {
                CustomViewCallback pending = callback;
                onHideCustomView();
                if (pending != null) pending.onCustomViewHidden();
            }
        };

        Client(Activity activity, WebView web, WebChromeClient qt) {
            this.activity = activity;
            this.web = web;
            this.qt = qt;
        }

        @Override
        public void onProgressChanged(WebView view, int progress) {
            if (qt != null) qt.onProgressChanged(view, progress);
        }

        @Override
        public void onReceivedIcon(WebView view, Bitmap icon) {
            if (qt != null) qt.onReceivedIcon(view, icon);
        }

        @Override
        public void onReceivedTitle(WebView view, String title) {
            if (qt != null) qt.onReceivedTitle(view, title);
        }

        @Override
        public void onGeolocationPermissionsShowPrompt(String origin, GeolocationPermissions.Callback callback) {
            if (qt != null) qt.onGeolocationPermissionsShowPrompt(origin, callback);
            else callback.invoke(origin, false, false);
        }

        @Override
        public void onShowCustomView(View view, CustomViewCallback callback) {
            if (cover != null) {
                callback.onCustomViewHidden();
                return;
            }
            this.callback = callback;
            cover = new FrameLayout(activity) {
                @Override
                public boolean dispatchKeyEvent(KeyEvent event) {
                    if (event.getKeyCode() != KeyEvent.KEYCODE_BACK) return super.dispatchKeyEvent(event);
                    CustomViewCallback leave = Client.this.callback;
                    if (event.getAction() == KeyEvent.ACTION_UP && leave != null) leave.onCustomViewHidden();
                    return true;
                }
            };
            cover.setBackgroundColor(Color.BLACK);
            cover.addView(view, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            decor().addView(cover, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            view.setFocusable(true);
            view.setFocusableInTouchMode(true);
            view.requestFocus();
            orientation = activity.getRequestedOrientation();
            activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
            barsShown = showBars(false);
            web.addOnAttachStateChangeListener(closed);
        }

        @Override
        public void onHideCustomView() {
            if (cover == null) return;
            web.removeOnAttachStateChangeListener(closed);
            decor().removeView(cover);
            cover.removeAllViews();
            cover = null;
            callback = null;
            activity.setRequestedOrientation(orientation);
            if (barsShown) showBars(true);
        }

        private ViewGroup decor() {
            return (ViewGroup) activity.getWindow().getDecorView();
        }

        // Shows or hides the status and navigation bars; answers whether they
        // were showing before.
        @SuppressWarnings("deprecation")
        private boolean showBars(boolean show) {
            View decor = activity.getWindow().getDecorView();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                WindowInsets insets = decor.getRootWindowInsets();
                boolean shown = insets == null || insets.isVisible(WindowInsets.Type.statusBars());
                WindowInsetsController bars = activity.getWindow().getInsetsController();
                if (bars != null) {
                    if (show) {
                        bars.show(WindowInsets.Type.systemBars());
                    } else {
                        bars.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                        bars.hide(WindowInsets.Type.systemBars());
                    }
                }
                return shown;
            }
            int flags = decor.getSystemUiVisibility();
            int hidden = View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            decor.setSystemUiVisibility(show ? flags & ~hidden : flags | hidden);
            return (flags & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0;
        }
    }
}
