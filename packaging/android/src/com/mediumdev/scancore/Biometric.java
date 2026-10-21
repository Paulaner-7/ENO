package com.mediumdev.scancore;

import android.content.Context;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.os.Build;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;

// Fingerprint or face for the app lock (src/app/Lock.cpp), through the
// system's own prompt. Android 11 and later also offer the phone's screen
// lock there, which is the way back in for a forgotten PIN.
public final class Biometric {
    private Biometric() {}

    static native void nativeResult(boolean ok);

    public static boolean available(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return false;
        BiometricManager manager = context.getSystemService(BiometricManager.class);
        if (manager == null) return false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            return manager.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_WEAK) == BiometricManager.BIOMETRIC_SUCCESS;
        return manager.canAuthenticate() == BiometricManager.BIOMETRIC_SUCCESS;
    }

    public static void prompt(final Context context, final String title, final String usePin) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            nativeResult(false);
            return;
        }
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                BiometricPrompt.Builder builder = new BiometricPrompt.Builder(context).setTitle(title);
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                    builder.setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_WEAK
                                                     | BiometricManager.Authenticators.DEVICE_CREDENTIAL);
                else
                    builder.setNegativeButton(usePin, context.getMainExecutor(), (dialog, which) -> nativeResult(false));
                builder.build().authenticate(new CancellationSignal(), context.getMainExecutor(),
                    new BiometricPrompt.AuthenticationCallback() {
                        @Override
                        public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result) {
                            nativeResult(true);
                        }

                        @Override
                        public void onAuthenticationError(int code, CharSequence message) {
                            nativeResult(false);
                        }
                    });
            } catch (RuntimeException e) {
                nativeResult(false);
            }
        });
    }
}
