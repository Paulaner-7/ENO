#pragma once

#include <QString>

namespace sc {

// While one lives (one source's part of a search across many sources), a
// browser check an extension meets on that site is not opened over the app:
// the source says it needs one, with its button in the results.
class QuietSite {
public:
    explicit QuietSite(const QString &siteUrl);
    ~QuietSite();
    QuietSite(const QuietSite &) = delete;
    QuietSite &operator=(const QuietSite &) = delete;

private:
    QString m_host;
};

// Whether a browser check for `url` should wait for its button.
bool isQuietSite(const QString &url);

} // namespace sc
