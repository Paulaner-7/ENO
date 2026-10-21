package com.mediumdev.scancore;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

// Opens a video in another player app (MX Player, VLC, mpv, Just Player…)
// through the system's chooser. MX Player and mpv-android read the headers,
// title, start position and subtitles from the extras; the others take what
// they understand.
public final class ExternalPlayer {
    private ExternalPlayer() {}

    public static boolean open(Activity activity, String url, String title, String[] headers, int positionMs,
                               String[] subtitles, String[] subtitleNames) {
        try {
            final Uri uri = Uri.parse(url);
            final Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setDataAndType(uri, url.contains(".m3u8") ? "application/x-mpegURL" : "video/*");
            intent.putExtra("title", title);
            intent.putExtra("headers", headers);
            intent.putExtra("position", positionMs);
            if (subtitles.length > 0) {
                final Uri[] subs = new Uri[subtitles.length];
                for (int i = 0; i < subtitles.length; ++i)
                    subs[i] = Uri.parse(subtitles[i]);
                intent.putExtra("subs", subs);
                intent.putExtra("subs.name", subtitleNames);
            }
            final Intent chooser = Intent.createChooser(intent, title);
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(chooser);
            return true;
        } catch (Exception e) {
            return false;
        }
    }
}
