package com.mediumdev.scancore;

import android.content.Context;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.speech.tts.Voice;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;

// Android's text-to-speech for SCANCORE's novel reader (src/app/Speech.cpp).
// Queued text is spoken by the system's engine, which runs in its own
// process, so a queued chapter keeps playing while the app is in the
// background. Utterance ids are SCANCORE's chunk numbers.
public final class Speech {
    private static TextToSpeech tts;
    private static boolean ready;
    // Text queued before the engine was ready: {chunk, text}.
    private static final ArrayList<Object[]> pending = new ArrayList<>();
    private static float rate = 1f, pitch = 1f;
    private static String voice = "";

    private Speech() {}

    static native void nativeReady(boolean ok, String voicesJson);
    static native void nativeStarted(int chunk);
    static native void nativeDone(int chunk);
    static native void nativeError(int chunk, String message);

    public static synchronized void init(Context context) {
        if (tts != null) return;
        tts = new TextToSpeech(context.getApplicationContext(), status -> {
            synchronized (Speech.class) {
                ready = status == TextToSpeech.SUCCESS;
                if (ready) {
                    tts.setOnUtteranceProgressListener(new UtteranceProgressListener() {
                        @Override public void onStart(String id) { nativeStarted(Integer.parseInt(id)); }
                        @Override public void onDone(String id) { nativeDone(Integer.parseInt(id)); }
                        @Override public void onStop(String id, boolean interrupted) {}
                        @Override public void onError(String id) { nativeError(Integer.parseInt(id), ""); }
                        @Override public void onError(String id, int code) { nativeError(Integer.parseInt(id), "Text-to-speech error " + code); }
                    });
                    apply();
                    for (Object[] p : pending) speakNow((String) p[1], (Integer) p[0]);
                }
                pending.clear();
                nativeReady(ready, voices());
            }
        });
    }

    private static String voices() {
        JSONArray out = new JSONArray();
        try {
            if (tts.getVoices() != null)
                for (Voice v : tts.getVoices()) {
                    if (v.isNetworkConnectionRequired() && v.getFeatures().contains(TextToSpeech.Engine.KEY_FEATURE_NOT_INSTALLED)) continue;
                    JSONObject o = new JSONObject();
                    o.put("id", v.getName());
                    o.put("name", v.getName());
                    o.put("language", v.getLocale().toLanguageTag().replace('-', '_'));
                    out.put(o);
                }
        } catch (Exception ignored) {
        }
        return out.toString();
    }

    private static void apply() {
        tts.setSpeechRate(rate);
        tts.setPitch(pitch);
        if (!voice.isEmpty() && tts.getVoices() != null)
            for (Voice v : tts.getVoices())
                if (v.getName().equals(voice)) { tts.setVoice(v); return; }
    }

    private static void speakNow(String text, int chunk) {
        tts.speak(text, TextToSpeech.QUEUE_ADD, new Bundle(), Integer.toString(chunk));
    }

    public static synchronized void speak(String text, int chunk) {
        if (tts == null) return;
        if (ready) speakNow(text, chunk);
        else pending.add(new Object[] {chunk, text});
    }

    public static synchronized void stop() {
        pending.clear();
        if (tts != null && ready) tts.stop();
    }

    public static synchronized void configure(String voiceName, float speechRate, float speechPitch) {
        voice = voiceName == null ? "" : voiceName;
        rate = speechRate;
        pitch = speechPitch;
        if (tts != null && ready) apply();
    }
}
