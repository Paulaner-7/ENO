#pragma once

#include <QString>
#include <QStringList>

namespace sc {

// Titles as search and matching compare them: lower case, accents dropped
// ("Pequeña" and "pequena" are the same), and every run of characters that
// are not letters or digits one space. Letters of any script are kept, so
// Japanese, Korean and Chinese titles stay whole.
QString normalizedTitle(const QString &text);

// The same without spaces, for exact matches ("Año 2" -> "ano2").
QString titleKey(const QString &text);

// A content language's name in that language, for source lists: "en" ->
// "English", "es" -> "Español".
QString languageName(const QString &code);

// The code of a language written by name, in English or in itself
// ("Español", "Spanish" -> "es"), as LNReader plugin lists write them; a
// code stays a code. Empty when unknown.
QString languageCode(const QString &name);

// The base of a language code: "es-la" and "es_419" -> "es", "pt_BR" -> "pt".
QString baseLanguage(const QString &code);

// The language content should be in: the "Languages for All sources"
// setting when set, else the interface language ("system" is the system
// locale), else English. Short code ("es", "pt"). Orders "Read from" and
// breaks ties in search; new repositories install extensions in it.
QString contentLanguage(const QString &sourceLanguages, const QString &interfaceLanguage, const QString &systemLocale);
// The same from the stored settings.
QString contentLanguage();

// The languages whose built-in sources are on (Settings -> Sources): the
// stored choice, else the content language alone. Short codes; "*" (every
// language) without stored settings. A medium with no built-in source in
// these languages keeps its English ones (the registries see to that).
QStringList builtinLanguages();

} // namespace sc
