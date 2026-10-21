package com.mediumdev.scancore;

import android.graphics.Bitmap;
import android.graphics.ImageDecoder;
import android.os.Build;

import java.nio.ByteBuffer;

// Images Qt cannot decode (AVIF), with the system's decoder (Android 12 and
// later), for src/app/Platform.cpp.
public final class Images {
    private Images() {}

    // [width, height, then ARGB pixels row by row], or null.
    public static int[] decode(byte[] data) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) return null;
        try {
            Bitmap bitmap = ImageDecoder.decodeBitmap(ImageDecoder.createSource(ByteBuffer.wrap(data)),
                (decoder, info, source) -> decoder.setAllocator(ImageDecoder.ALLOCATOR_SOFTWARE));
            int width = bitmap.getWidth();
            int height = bitmap.getHeight();
            int[] out = new int[2 + width * height];
            out[0] = width;
            out[1] = height;
            bitmap.getPixels(out, 2, width, 0, 0, width, height);
            bitmap.recycle();
            return out;
        } catch (Exception | OutOfMemoryError e) {
            return null;
        }
    }
}
