#include "WebBridge.h"

#ifdef SCANCORE_HAVE_WEBENGINE
#include "core/Cookies.h"
#include "core/Http.h"

#include <QCoreApplication>
#include <QNetworkCookie>
#include <QPointer>
#include <QTimer>
#include <QWebEngineCookieStore>
#include <QWebEngineHttpRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QtWebEngineQuick/QQuickWebEngineProfile>

#include <algorithm>
#include <deque>
#endif

namespace sc {

namespace {
std::function<void(const QString &)> g_clearanceSeen;
}

void setClearanceSeen(std::function<void(const QString &domain)> seen) { g_clearanceSeen = std::move(seen); }

void installWebBridge() {
#ifdef SCANCORE_HAVE_WEBENGINE
    static bool installed = false;
    if (installed)
        return;
    installed = true;
    auto *profile = QQuickWebEngineProfile::defaultProfile();
    if (!profile)
        return;
    profile->setHttpUserAgent(QString::fromUtf8(Http::userAgent()));
    auto *store = profile->cookieStore();
    QObject::connect(store, &QWebEngineCookieStore::cookieAdded, &CookieJar::shared(), [](const QNetworkCookie &cookie) {
        // Only a clearance the jar did not have is news: the stored ones
        // come through here too when the browser starts.
        bool fresh = false;
        if (cookie.name() == "cf_clearance") {
            const auto domain = cookie.domain().startsWith(QLatin1Char('.')) ? cookie.domain().mid(1) : cookie.domain();
            const auto known = CookieJar::shared().cookiesForUrl(QUrl(QStringLiteral("https://") + domain + cookie.path()));
            fresh = std::none_of(known.begin(), known.end(), [&](const QNetworkCookie &c) { return c.name() == cookie.name() && c.value() == cookie.value(); });
        }
        CookieJar::shared().insertCookie(cookie);
        CookieJar::shared().save();
        if (fresh && g_clearanceSeen)
            g_clearanceSeen(cookie.domain().startsWith(QLatin1Char('.')) ? cookie.domain().mid(1) : cookie.domain());
    });
    QObject::connect(store, &QWebEngineCookieStore::cookieRemoved, &CookieJar::shared(),
                     [](const QNetworkCookie &cookie) { CookieJar::shared().deleteCookie(cookie); });
    store->loadAllCookies();
#endif
}

#ifdef SCANCORE_HAVE_WEBENGINE
namespace {

struct Check {
    QUrl url;
    QString userAgent;
    QJsonObject headers;
    std::function<void(const QJsonArray &)> done;
};

// Cloudflare's challenge page posts its progress to the window; a check
// that wants a tap says "interactiveBegin". Run in the page's own world, as
// it has to see the page's messages; it only reports them.
const char *const kListener = R"(
addEventListener("message", (e) => {
    const d = e.data;
    if (d && d.source === "cloudflare-challenge") console.info("scancore-check:" + d.event);
});
)";

// Whether the page is a challenge at all (else there is nothing to pass).
const char *const kIsChallenge = R"(
!!(document.querySelector("#challenge-form, #challenge-running, #challenge-stage, [name=cf-turnstile-response], script[src*='challenge-platform']")
   || /just a moment/i.test(document.title))
)";

bool cookieFor(const QNetworkCookie &cookie, const QString &host) {
    const auto domain = cookie.domain().startsWith(QLatin1Char('.')) ? cookie.domain().mid(1) : cookie.domain();
    return host == domain || host.endsWith(QLatin1Char('.') + domain);
}

class CheckPage : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;
    std::function<void(const QString &)> onEvent;

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel, const QString &message, int, const QString &) override {
        if (message.startsWith(QLatin1String("scancore-check:")) && onEvent)
            onEvent(message.mid(int(qstrlen("scancore-check:"))));
    }
};

class Checks : public QObject {
public:
    Checks() : QObject(QCoreApplication::instance()) {
        // Off the record: no clearance left from an earlier check is sent.
        m_profile = new QWebEngineProfile(this);
        QWebEngineScript listener;
        listener.setName(QStringLiteral("scancore-check"));
        listener.setSourceCode(QString::fromUtf8(kListener));
        listener.setInjectionPoint(QWebEngineScript::DocumentCreation);
        listener.setWorldId(QWebEngineScript::MainWorld);
        listener.setRunsOnSubFrames(false);
        m_profile->scripts()->insert(listener);
        m_timeout.setSingleShot(true);
        connect(&m_timeout, &QTimer::timeout, this, [this] { finish(false); });
        connect(m_profile->cookieStore(), &QWebEngineCookieStore::cookieAdded, this, [this](const QNetworkCookie &cookie) {
            // SCANCORE's own requests to the site pass too.
            CookieJar::shared().insertCookie(cookie);
            CookieJar::shared().save();
            if (m_page && cookie.name() == "cf_clearance" && cookieFor(cookie, m_current.url.host()))
                finish(true);
        });
    }

    void add(Check check) {
        m_queue.push_back(std::move(check));
        if (!m_page)
            next();
    }

private:
    void next() {
        if (m_queue.empty())
            return;
        m_current = std::move(m_queue.front());
        m_queue.pop_front();
        m_profile->setHttpUserAgent(m_current.userAgent.isEmpty() ? QString::fromUtf8(Http::userAgent()) : m_current.userAgent);
        auto *page = new CheckPage(m_profile, this);
        m_page = page;
        page->onEvent = [this](const QString &event) {
            if (event == QLatin1String("interactiveBegin") || event == QLatin1String("fail"))
                finish(false);
        };
        connect(page, &QWebEnginePage::loadFinished, this, [this, page](bool) {
            page->runJavaScript(QString::fromUtf8(kIsChallenge), QWebEngineScript::ApplicationWorld, [this, page](const QVariant &challenge) {
                // A page without a check: give a clearance on its way a moment.
                if (m_page == page && !challenge.toBool())
                    QTimer::singleShot(1500, this, [this, page] {
                        if (m_page == page)
                            finish(false);
                    });
            });
        });
        QWebEngineHttpRequest request(m_current.url);
        for (auto it = m_current.headers.begin(); it != m_current.headers.end(); ++it)
            request.setHeader(it.key().toUtf8(), it.value().toString().toUtf8());
        page->load(request);
        m_timeout.start(30000);
    }

    void finish(bool passed) {
        if (!m_page)
            return;
        m_timeout.stop();
        m_page->deleteLater();
        m_page = nullptr;
        QJsonArray cookies;
        if (passed)
            for (const auto &cookie : CookieJar::shared().cookiesForUrl(m_current.url))
                cookies.append(QString::fromUtf8(cookie.toRawForm()));
        auto done = std::move(m_current.done);
        m_current = {};
        if (done)
            done(cookies);
        next();
    }

    QWebEngineProfile *m_profile = nullptr;
    QPointer<QWebEnginePage> m_page;
    std::deque<Check> m_queue;
    Check m_current;
    QTimer m_timeout;
};

} // namespace
#endif

void solveBrowserCheck(const QUrl &url, const QString &userAgent, const QJsonObject &headers, std::function<void(const QJsonArray &cookies)> done) {
#ifdef SCANCORE_HAVE_WEBENGINE
    static Checks *checks = nullptr;
    if (!checks)
        checks = new Checks();
    checks->add({url, userAgent, headers, std::move(done)});
#else
    Q_UNUSED(url)
    Q_UNUSED(userAgent)
    Q_UNUSED(headers)
    done({});
#endif
}

} // namespace sc
