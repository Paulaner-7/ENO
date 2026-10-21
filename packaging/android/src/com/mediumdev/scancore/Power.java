package com.mediumdev.scancore;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.PowerManager;
import android.provider.Settings;

// Battery optimisation (src/app/Platform.cpp). Android stops optimised apps
// soon after they leave the screen, which ends library update checks,
// downloads and reading aloud; SCANCORE asks the reader to let it run.
public final class Power {
    private Power() {}

    public static boolean restricted(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        PowerManager power = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        return power != null && !power.isIgnoringBatteryOptimizations(context.getPackageName());
    }

    // Plugged in (or on a wireless charger), for "only while charging".
    public static boolean charging(Context context) {
        android.os.BatteryManager battery = (android.os.BatteryManager) context.getSystemService(Context.BATTERY_SERVICE);
        return battery == null || battery.isCharging();
    }

    // The system's own "Allow SCANCORE to always run in the background?"
    // question, or its battery optimisation list where that is missing.
    public static void ask(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
        Intent ask = new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
                                Uri.parse("package:" + context.getPackageName()));
        Intent list = new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS);
        if (!(context instanceof android.app.Activity)) {
            ask.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            list.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        }
        try {
            context.startActivity(ask);
        } catch (ActivityNotFoundException | SecurityException e) {
            try {
                context.startActivity(list);
            } catch (ActivityNotFoundException ignored) {
            }
        }
    }
}
