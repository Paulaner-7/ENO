package com.mediumdev.scancore;

import android.app.Activity;
import android.content.Intent;

// The system's share sheet for a text (a crash report to send).
public final class Share {
    private Share() {}

    public static boolean text(Activity activity, String text, String subject) {
        try {
            final Intent intent = new Intent(Intent.ACTION_SEND);
            intent.setType("text/plain");
            intent.putExtra(Intent.EXTRA_SUBJECT, subject);
            intent.putExtra(Intent.EXTRA_TEXT, text);
            final Intent chooser = Intent.createChooser(intent, subject);
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(chooser);
            return true;
        } catch (Exception e) {
            return false;
        }
    }
}
