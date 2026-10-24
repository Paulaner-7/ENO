package com.mediumdev.scancore;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

// System notifications for SCANCORE's notification area
// (src/app/Notifications.cpp): new chapters, announcements and updates.
// Tapping one opens the app, whose own panel lists them all.
public final class Notify {
    private static final String CHANNEL = "updates";
    private static final String PERMISSION = "android.permission.POST_NOTIFICATIONS";

    private Notify() {}

    public static boolean allowed(Context context) {
        return Build.VERSION.SDK_INT < 33 || context.checkSelfPermission(PERMISSION) == PackageManager.PERMISSION_GRANTED;
    }

    // Android 13 and later ask the reader once.
    public static void ask(Context context) {
        if (Build.VERSION.SDK_INT >= 33 && context instanceof Activity && !allowed(context))
            ((Activity) context).requestPermissions(new String[] {PERMISSION}, 4711);
    }

    public static void show(Context context, String title, String body, String tag, String channelName) {
        if (!allowed(context)) return;
        NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager == null) return;
        if (Build.VERSION.SDK_INT >= 26 && manager.getNotificationChannel(CHANNEL) == null)
            manager.createNotificationChannel(new NotificationChannel(CHANNEL, channelName, NotificationManager.IMPORTANCE_DEFAULT));
        Intent open = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        PendingIntent tap = null;
        if (open != null) {
            open.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            tap = PendingIntent.getActivity(context, 0, open, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        }
        Notification.Builder builder = Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(context, CHANNEL) : new Notification.Builder(context);
        builder.setSmallIcon(R.drawable.ic_notification)
            .setContentTitle(title)
            .setContentText(body)
            .setStyle(new Notification.BigTextStyle().bigText(body))
            .setAutoCancel(true);
        if (tap != null) builder.setContentIntent(tap);
        manager.notify(tag, 0, builder.build());
    }
}
