package com.mediumdev.scancore;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;

import com.google.android.gms.tasks.Tasks;
import com.google.mlkit.common.model.DownloadConditions;
import com.google.mlkit.nl.translate.TranslateLanguage;
import com.google.mlkit.nl.translate.Translation;
import com.google.mlkit.nl.translate.Translator;
import com.google.mlkit.nl.translate.TranslatorOptions;
import com.google.mlkit.vision.common.InputImage;
import com.google.mlkit.vision.text.Text;
import com.google.mlkit.vision.text.TextRecognition;
import com.google.mlkit.vision.text.TextRecognizer;
import com.google.mlkit.vision.text.chinese.ChineseTextRecognizerOptions;
import com.google.mlkit.vision.text.japanese.JapaneseTextRecognizerOptions;
import com.google.mlkit.vision.text.korean.KoreanTextRecognizerOptions;
import com.google.mlkit.vision.text.latin.TextRecognizerOptions;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.concurrent.TimeUnit;

// On-device page translation on Android (src/app/Translator.cpp): Google's
// ML Kit reads the text of a page and translates it, with models Play
// services keeps on the phone (downloaded the first time a language is
// used). Calls block; SCANCORE makes them on worker threads. Failures come
// back as {"error": message}.
public final class MlKit {
    private MlKit() {}

    private static String base(String language) {
        if (language == null) return "";
        return language.split("[-_]")[0].toLowerCase();
    }

    // The page's lines: [{x, y, w, h, text}] in the picture's pixels.
    public static String recognize(byte[] image, String language) {
        try {
            Bitmap bitmap = BitmapFactory.decodeByteArray(image, 0, image.length);
            if (bitmap == null) return error("The picture could not be read.");
            TextRecognizer recognizer;
            switch (base(language)) {
                case "ko": recognizer = TextRecognition.getClient(new KoreanTextRecognizerOptions.Builder().build()); break;
                case "ja": recognizer = TextRecognition.getClient(new JapaneseTextRecognizerOptions.Builder().build()); break;
                case "zh": recognizer = TextRecognition.getClient(new ChineseTextRecognizerOptions.Builder().build()); break;
                default: recognizer = TextRecognition.getClient(TextRecognizerOptions.DEFAULT_OPTIONS);
            }
            try {
                Text text = Tasks.await(recognizer.process(InputImage.fromBitmap(bitmap, 0)), 90, TimeUnit.SECONDS);
                JSONArray out = new JSONArray();
                for (Text.TextBlock block : text.getTextBlocks())
                    for (Text.Line line : block.getLines()) {
                        Rect box = line.getBoundingBox();
                        if (box == null) continue;
                        JSONObject o = new JSONObject();
                        o.put("x", box.left);
                        o.put("y", box.top);
                        o.put("w", box.width());
                        o.put("h", box.height());
                        o.put("text", line.getText());
                        out.put(o);
                    }
                return out.toString();
            } finally {
                recognizer.close();
                bitmap.recycle();
            }
        } catch (Exception e) {
            return error(message(e));
        }
    }

    // ["...", ...] translated, the language's model fetched first if need be.
    public static String translate(String textsJson, String from, String to) {
        try {
            String source = TranslateLanguage.fromLanguageTag(base(from));
            String target = TranslateLanguage.fromLanguageTag(base(to));
            if (source == null || target == null)
                return error("Android's translator does not know this language.");
            TranslatorOptions options = new TranslatorOptions.Builder().setSourceLanguage(source).setTargetLanguage(target).build();
            Translator translator = Translation.getClient(options);
            try {
                Tasks.await(translator.downloadModelIfNeeded(new DownloadConditions.Builder().build()), 10, TimeUnit.MINUTES);
                JSONArray texts = new JSONArray(textsJson);
                JSONArray out = new JSONArray();
                for (int i = 0; i < texts.length(); i++)
                    out.put(Tasks.await(translator.translate(texts.getString(i)), 60, TimeUnit.SECONDS));
                return out.toString();
            } finally {
                translator.close();
            }
        } catch (Exception e) {
            return error(message(e));
        }
    }

    private static String message(Exception e) {
        Throwable t = e.getCause() != null ? e.getCause() : e;
        String m = t.getMessage();
        return m == null || m.isEmpty() ? t.getClass().getSimpleName() : m;
    }

    private static String error(String message) {
        try {
            return new JSONObject().put("error", message).toString();
        } catch (Exception e) {
            return "{\"error\":\"failed\"}";
        }
    }
}
