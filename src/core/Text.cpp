#include "Text.h"

#include "Database.h"

#include <QHash>
#include <QList>
#include <QLocale>
#include <QRegularExpression>

namespace sc {

QString normalizedTitle(const QString &text) {
    const auto decomposed = text.toLower().normalized(QString::NormalizationForm_D).toUcs4();
    QList<char32_t> out;
    out.reserve(decomposed.size());
    bool gap = false;
    for (const char32_t c : decomposed) {
        // Accents on Latin, Greek and Cyrillic letters. Other scripts' marks
        // (Japanese voicing, Indic vowel signs) change the word and stay.
        if (c >= 0x0300 && c <= 0x036F)
            continue;
        if (QChar::isLetterOrNumber(c) || QChar::isMark(c)) {
            if (gap && !out.isEmpty())
                out.append(U' ');
            gap = false;
            out.append(c);
        } else {
            gap = true;
        }
    }
    return QString::fromUcs4(out.constData(), out.size()).normalized(QString::NormalizationForm_C);
}

QString languageName(const QString &code) {
    static const QHash<QString, QString> names{
        {"en", "English"},  {"es", "Español"},   {"fr", "Français"}, {"pt", "Português"}, {"id", "Bahasa Indonesia"},
        {"it", "Italiano"}, {"de", "Deutsch"},   {"ru", "Русский"},  {"tr", "Türkçe"},    {"vi", "Tiếng Việt"},
        {"ar", "العربية"},  {"ja", "日本語"},    {"ko", "한국어"},   {"zh", "中文"},      {"th", "ไทย"},
        {"pl", "Polski"},   {"multi", "Multi"}};
    if (code.isEmpty())
        return names.value(QStringLiteral("en"));
    return names.value(code.section(QLatin1Char('-'), 0, 0).toLower(), code);
}

QString languageCode(const QString &name) {
    static const QHash<QString, QString> codes{
        {"english", "en"},    {"español", "es"},   {"spanish", "es"},    {"français", "fr"}, {"french", "fr"},
        {"português", "pt"},  {"portuguese", "pt"}, {"bahasa indonesia", "id"}, {"indonesian", "id"},
        {"italiano", "it"},   {"italian", "it"},   {"deutsch", "de"},    {"german", "de"},   {"русский", "ru"},
        {"russian", "ru"},    {"türkçe", "tr"},    {"turkish", "tr"},    {"tiếng việt", "vi"}, {"vietnamese", "vi"},
        {"العربية", "ar"},    {"arabic", "ar"},    {"日本語", "ja"},     {"japanese", "ja"}, {"한국어", "ko"},
        {"korean", "ko"},     {"中文", "zh"},       {"chinese", "zh"},    {"ไทย", "th"},      {"thai", "th"},
        {"polski", "pl"},     {"polish", "pl"},    {"українська", "uk"}, {"ukrainian", "uk"}, {"multi", "multi"}};
    // Marks some lists put around names (left-to-right marks, "(简体)").
    auto key = name;
    key.remove(QChar(0x200e)).remove(QChar(0x200f));
    key = key.section(QLatin1Char('('), 0, 0).trimmed().toLower();
    if (const auto code = codes.value(key); !code.isEmpty())
        return code;
    static const QRegularExpression code(QStringLiteral("^[a-z]{2,3}([-_][A-Za-z0-9]+)?$"));
    return code.match(key).hasMatch() ? key : QString();
}

QString baseLanguage(const QString &code) {
    return code.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).trimmed().toLower();
}

QString contentLanguage(const QString &sourceLanguages, const QString &interfaceLanguage, const QString &systemLocale) {
    for (const auto &candidate : {sourceLanguages.section(QLatin1Char(','), 0, 0),
                                  interfaceLanguage == QLatin1String("system") ? systemLocale : interfaceLanguage}) {
        const auto base = baseLanguage(candidate);
        // The C locale ("C", "c") stands for no language.
        if (base.size() >= 2 && base != QLatin1String("system"))
            return base;
    }
    return QStringLiteral("en");
}

QString contentLanguage() {
    return contentLanguage(Database::pref(QStringLiteral("setting.sourceLanguages")).toString(),
                           Database::pref(QStringLiteral("language"), QStringLiteral("system")).toString(),
                           QLocale::system().name());
}

QStringList builtinLanguages() {
    // No settings (tools such as the probe): every language.
    if (Database::path().isEmpty())
        return {QStringLiteral("*")};
    const auto stored = Database::pref(QStringLiteral("setting.builtinLanguages"));
    QStringList out;
    for (const auto &code : stored.toStringList())
        if (const auto base = baseLanguage(code); !base.isEmpty() && !out.contains(base))
            out.append(base);
    if (!out.isEmpty())
        return out;
    // By default only the content language's (which follows the interface
    // language).
    return {contentLanguage()};
}

QString titleKey(const QString &text) { return normalizedTitle(text).remove(QLatin1Char(' ')); }

} // namespace sc
