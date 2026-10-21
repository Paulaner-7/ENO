package com.mediumdev.scancore;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

// A site's browser check on a phone (src/app/Platform.cpp). The site opens
// in Android's web view with SCANCORE's user agent; when the reader presses
// Done (or Back) the cookies the site set go to SCANCORE and to extensions,
// so their requests pass the check too.
public final class BrowserCheck extends Activity {
    private WebView web;
    private String url;
    private boolean reported;
    // A sign-in that hands back a token in its address (AniList): the
    // address to report instead of the one it started at.
    private String handedBack;

    static native void nativeDone(String url, String cookies);

    public static void start(Context context, String url, String userAgent, String title, String done) {
        Intent intent = new Intent(context, BrowserCheck.class)
            .putExtra("url", url)
            .putExtra("userAgent", userAgent)
            .putExtra("title", title)
            .putExtra("done", done);
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    private int dp(float value) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics()));
    }

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        Intent intent = getIntent();
        url = intent.getStringExtra("url");

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.rgb(10, 10, 10));
        root.setFitsSystemWindows(true);

        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setPadding(dp(16), dp(8), dp(8), dp(8));
        bar.setBackgroundColor(Color.rgb(20, 20, 20));
        TextView title = new TextView(this);
        title.setText(intent.getStringExtra("title"));
        title.setTextColor(Color.rgb(230, 230, 230));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        title.setMaxLines(3);
        bar.addView(title, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        Button done = new Button(this);
        done.setText(intent.getStringExtra("done"));
        done.setAllCaps(false);
        done.setOnClickListener(v -> finish());
        bar.addView(done, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        root.addView(bar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        web = new WebView(this);
        WebSettings settings = web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        String userAgent = intent.getStringExtra("userAgent");
        if (userAgent != null && !userAgent.isEmpty()) settings.setUserAgentString(userAgent);
        CookieManager cookies = CookieManager.getInstance();
        cookies.setAcceptCookie(true);
        cookies.setAcceptThirdPartyCookies(web, true);
        // Links and the check's own redirects stay in this view. Once the
        // site sets a new Cloudflare clearance the check is passed, and the
        // screen closes by itself.
        final String before = clearance(cookies, url);
        web.setWebViewClient(new WebViewClient());
        // Looked for four times a second: a site's page behind the check
        // may never quite finish loading (ads, covers), and waiting for it
        // kept the screen open long after the check had passed.
        final android.os.Handler main = new android.os.Handler(android.os.Looper.getMainLooper());
        main.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (isFinishing() || isDestroyed()) return;
                String now = clearance(cookies, url);
                String address = web.getUrl();
                if (address != null && address.contains("access_token=")) {
                    handedBack = address;
                    finish();
                } else if (now != null && !now.equals(before)) finish();
                else main.postDelayed(this, 250);
            }
        }, 250);
        web.setOverScrollMode(View.OVER_SCROLL_NEVER);
        root.addView(web, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        setContentView(root);
        if (state == null) web.loadUrl(url);
        else web.restoreState(state);
    }

    @Override
    protected void onSaveInstanceState(Bundle state) {
        super.onSaveInstanceState(state);
        if (web != null) web.saveState(state);
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        if (web != null && web.canGoBack()) web.goBack();
        else finish();
    }

    private static String clearance(CookieManager cookies, String url) {
        String header = url == null ? null : cookies.getCookie(url);
        if (header == null) return null;
        for (String part : header.split(";")) {
            String p = part.trim();
            if (p.startsWith("cf_clearance=")) return p.substring("cf_clearance=".length());
        }
        return null;
    }

    private void report() {
        if (reported || url == null) return;
        reported = true;
        CookieManager cookies = CookieManager.getInstance();
        cookies.flush();
        String header = cookies.getCookie(url);
        nativeDone(handedBack != null ? handedBack : url, header == null ? "" : header);
    }

    @Override
    public void finish() {
        report();
        super.finish();
    }

    @Override
    protected void onDestroy() {
        // Leaving any other way (the system closing it) still hands over
        // what the site set.
        if (isFinishing()) report();
        if (web != null) {
            web.stopLoading();
            web.destroy();
            web = null;
        }
        super.onDestroy();
    }
}
