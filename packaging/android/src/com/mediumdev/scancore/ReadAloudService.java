package com.mediumdev.scancore;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.IBinder;

// Reading a novel aloud with the screen off (src/app/ReadAloud.cpp through
// src/app/MediaControls.cpp): a media-playback foreground service keeps
// SCANCORE's process running between chapters, and its media session puts
// play/pause and skip on the lock screen, in the notification and on
// headphones' buttons. The commands go back to the app as words ("play",
// "pause", "toggle", "next", "previous", "stop").
public final class ReadAloudService extends Service {
    private static final String CHANNEL = "read-aloud";
    private static final int ID = 4712;
    private static final String COMMAND = "com.mediumdev.scancore.READ_ALOUD";

    // What the notification shows; kept for a service that (re)starts.
    private static boolean playing;
    private static String title = "", subtitle = "", channelName = "Reading aloud";
    private static ReadAloudService running;
    private MediaSession session;

    static native void nativeCommand(String command);

    // From the app: show (start or update) or hide the controls.
    public static synchronized void show(Context context, boolean active, boolean isPlaying, String bookTitle, String chapterTitle,
                                         String channelLabel) {
        playing = isPlaying;
        title = bookTitle == null ? "" : bookTitle;
        subtitle = chapterTitle == null ? "" : chapterTitle;
        if (channelLabel != null && !channelLabel.isEmpty()) channelName = channelLabel;
        Context app = context.getApplicationContext();
        if (!active) {
            if (running != null) running.finish();
            return;
        }
        if (running != null) {
            running.refresh();
            return;
        }
        // Started only while reading, which the reader began on screen.
        Intent intent = new Intent(app, ReadAloudService.class);
        try {
            if (Build.VERSION.SDK_INT >= 26) app.startForegroundService(intent);
            else app.startService(intent);
        } catch (RuntimeException e) {
            // Android 12+ refuses a start from the background; the next
            // change made on screen starts it.
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        synchronized (ReadAloudService.class) {
            running = this;
        }
        session = new MediaSession(this, "SCANCORE");
        session.setCallback(new MediaSession.Callback() {
            @Override public void onPlay() { nativeCommand("play"); }
            @Override public void onPause() { nativeCommand("pause"); }
            @Override public void onSkipToNext() { nativeCommand("next"); }
            @Override public void onSkipToPrevious() { nativeCommand("previous"); }
            @Override public void onStop() { nativeCommand("stop"); }
        });
        session.setActive(true);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String command = intent != null ? intent.getStringExtra(COMMAND) : null;
        if (command != null) nativeCommand(command);
        startInForeground();
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    @Override
    public void onDestroy() {
        synchronized (ReadAloudService.class) {
            if (running == this) running = null;
        }
        if (session != null) session.release();
        super.onDestroy();
    }

    private void finish() {
        stopForeground(STOP_FOREGROUND_REMOVE);
        stopSelf();
    }

    private void refresh() {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        updateSession();
        if (manager != null) manager.notify(ID, build());
    }

    private void startInForeground() {
        updateSession();
        Notification n = build();
        if (Build.VERSION.SDK_INT >= 29) startForeground(ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        else startForeground(ID, n);
    }

    private void updateSession() {
        session.setMetadata(new MediaMetadata.Builder()
            .putString(MediaMetadata.METADATA_KEY_TITLE, subtitle)
            .putString(MediaMetadata.METADATA_KEY_ALBUM, title)
            .putString(MediaMetadata.METADATA_KEY_ARTIST, title)
            .build());
        long actions = PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PAUSE | PlaybackState.ACTION_PLAY_PAUSE
            | PlaybackState.ACTION_SKIP_TO_NEXT | PlaybackState.ACTION_SKIP_TO_PREVIOUS | PlaybackState.ACTION_STOP;
        session.setPlaybackState(new PlaybackState.Builder()
            .setActions(actions)
            .setState(playing ? PlaybackState.STATE_PLAYING : PlaybackState.STATE_PAUSED, PlaybackState.PLAYBACK_POSITION_UNKNOWN, 1f)
            .build());
    }

    private PendingIntent command(String word, int code) {
        Intent intent = new Intent(this, ReadAloudService.class).putExtra(COMMAND, word);
        return PendingIntent.getService(this, code, intent, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
    }

    private Notification build() {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (Build.VERSION.SDK_INT >= 26 && manager != null && manager.getNotificationChannel(CHANNEL) == null) {
            NotificationChannel channel = new NotificationChannel(CHANNEL, channelName, NotificationManager.IMPORTANCE_LOW);
            channel.setShowBadge(false);
            manager.createNotificationChannel(channel);
        }
        Intent open = getPackageManager().getLaunchIntentForPackage(getPackageName());
        PendingIntent tap = null;
        if (open != null) {
            open.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            tap = PendingIntent.getActivity(this, 0, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        }
        Notification.Builder b = Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(this, CHANNEL) : new Notification.Builder(this);
        b.setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(subtitle.isEmpty() ? title : subtitle)
            .setContentText(title)
            .setOngoing(playing)
            .setShowWhen(false)
            .setVisibility(Notification.VISIBILITY_PUBLIC)
            .addAction(new Notification.Action.Builder(android.R.drawable.ic_media_previous, "Back", command("previous", 1)).build())
            .addAction(playing
                ? new Notification.Action.Builder(android.R.drawable.ic_media_pause, "Pause", command("pause", 2)).build()
                : new Notification.Action.Builder(android.R.drawable.ic_media_play, "Play", command("play", 3)).build())
            .addAction(new Notification.Action.Builder(android.R.drawable.ic_media_next, "Next", command("next", 4)).build())
            .addAction(new Notification.Action.Builder(android.R.drawable.ic_menu_close_clear_cancel, "Stop", command("stop", 5)).build())
            .setStyle(new Notification.MediaStyle().setMediaSession(session.getSessionToken()).setShowActionsInCompactView(0, 1, 2));
        if (tap != null) b.setContentIntent(tap);
        b.setDeleteIntent(command("stop", 6));
        return b.build();
    }
}
