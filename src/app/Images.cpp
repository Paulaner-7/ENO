#include "Images.h"

#include "core/ImageCache.h"

#include <QBuffer>
#include <QCache>
#include <QFile>
#include <QImageReader>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSvgRenderer>
#include <QThreadPool>

namespace sc {

namespace {

QThreadPool &coverPool() {
    static QThreadPool *pool = [] {
        auto *p = new QThreadPool;
        p->setMaxThreadCount(8);
        return p;
    }();
    return *pool;
}

QThreadPool &panelPool() {
    static QThreadPool *pool = [] {
        auto *p = new QThreadPool;
        p->setMaxThreadCount(6);
        return p;
    }();
    return *pool;
}

QString b64(const QString &value) {
    return QString::fromLatin1(value.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString unb64(const QString &value) {
    return QString::fromUtf8(QByteArray::fromBase64(value.toLatin1(), QByteArray::Base64UrlEncoding));
}

QImage decode(const QByteArray &data, const QSize &requested, int maxWidth = 0, int cornerRadius = 0) {
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const auto size = reader.size();
    if (size.isValid()) {
        QSize target = size;
        if (requested.width() > 0 && requested.height() > 0)
            target = size.scaled(requested, Qt::KeepAspectRatioByExpanding);
        else if (requested.width() > 0 && size.width() > requested.width())
            target = QSize(requested.width(), size.height() * requested.width() / size.width());
        else if (requested.height() > 0 && size.height() > requested.height())
            target = QSize(size.width() * requested.height() / size.height(), requested.height());
        if (maxWidth > 0 && target.width() > maxWidth)
            target = QSize(maxWidth, target.height() * maxWidth / target.width());
        if (target != size && target.width() < size.width())
            reader.setScaledSize(target);
    }
    QImage image = reader.read();
    if (image.isNull())
        throw Error(QStringLiteral("This image format could not be read."));
    if (cornerRadius > 0 && requested.width() > 0 && requested.height() > 0) {
        if (image.width() < requested.width() || image.height() < requested.height())
            image = image.scaled(requested, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int cropX = std::max(0, (image.width() - requested.width()) / 2);
        const int cropY = std::max(0, (image.height() - requested.height()) / 2);
        QImage rounded(requested, QImage::Format_ARGB32_Premultiplied);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
        QPainterPath path;
        const qreal r = std::min<qreal>(cornerRadius, std::min(requested.width(), requested.height()) / 2.0);
        path.addRoundedRect(QRectF(0, 0, requested.width(), requested.height()), r, r);
        painter.setClipPath(path);
        painter.drawImage(QPoint(0, 0), image, QRect(cropX, cropY, requested.width(), requested.height()));
        return rounded;
    }
    const auto targetFormat = image.hasAlphaChannel() ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;
    if (image.format() != targetFormat)
        image = image.convertToFormat(targetFormat);
    return image;
}

QMutex &decodedCacheMutex() {
    static QMutex mutex;
    return mutex;
}

// 64 MB of already-decoded covers and banners in RAM so scrolling shelves,
// switching sections and opening details pages do not re-decode JPEGs/WebPs.
QCache<QString, QImage> &decodedCache() {
    static QCache<QString, QImage> cache(64 * 1024 * 1024);
    return cache;
}

class RemoteResponse : public QQuickImageResponse, public QRunnable {
public:
    RemoteResponse(QString url, QString referer, QSize size, int cornerRadius = 0)
        : m_url(std::move(url)), m_referer(std::move(referer)), m_size(size), m_cornerRadius(cornerRadius),
          m_cancel(std::make_shared<std::atomic_bool>(false)) {
        setAutoDelete(false);
    }

    bool tryFromMemory() {
        const auto key = cacheKey();
        QMutexLocker lock(&decodedCacheMutex());
        if (const auto *cached = decodedCache().object(key)) {
            m_image = *cached;
            return true;
        }
        return false;
    }

    void finishImmediately() { emit finished(); }

    void run() override {
        CancelScope scope(m_cancel);
        const auto key = cacheKey();
        try {
            m_image = decode(ImageCache::fetch(m_url, m_referer), m_size, 0, m_cornerRadius);
            if (!m_image.isNull()) {
                QMutexLocker lock(&decodedCacheMutex());
                const int cost = int(std::min<qsizetype>(m_image.sizeInBytes(), 16 * 1024 * 1024));
                decodedCache().insert(key, new QImage(m_image), std::max(1, cost));
            }
        } catch (const Error &e) {
            m_error = e.message();
        } catch (const std::exception &e) {
            m_error = QString::fromUtf8(e.what());
        }
        emit finished();
    }

    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(m_image); }
    QString errorString() const override { return m_error; }
    void cancel() override { m_cancel->store(true); }

private:
    QString cacheKey() const {
        return QStringLiteral("%1#%2x%3r%4").arg(m_url).arg(m_size.width()).arg(m_size.height()).arg(m_cornerRadius);
    }

    QString m_url;
    QString m_referer;
    QSize m_size;
    int m_cornerRadius = 0;
    QImage m_image;
    QString m_error;
    CancelFlag m_cancel;
};

constexpr int kTileHeight = 2048;

} // namespace

QString RemoteImageProvider::encode(const QString &url, const QString &referer) {
    return QStringLiteral("image://remote/%1~%2").arg(b64(url), b64(referer));
}

QQuickImageResponse *RemoteImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize) {
    const auto parts = id.split(QLatin1Char('~'));
    const int cornerRadius = parts.size() > 2 ? parts.value(2).toInt() : 0;
    auto *response = new RemoteResponse(unb64(parts.value(0)), unb64(parts.value(1)), requestedSize, cornerRadius);
    if (response->tryFromMemory()) {
        QMetaObject::invokeMethod(response, &RemoteResponse::finishImmediately, Qt::QueuedConnection);
        return response;
    }
    coverPool().start(response);
    return response;
}

// ---------------------------------------------------------------- Icons

IconProvider::IconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    static QMutex mutex;
    static QHash<QString, QByteArray> sources;
    static QHash<QString, QImage> rendered;
    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(48, 48);
    const QString renderKey = QStringLiteral("%1@%2x%3").arg(id).arg(target.width()).arg(target.height());
    const auto name = id.section(QLatin1Char('?'), 0, 0);
    const auto query = QUrlQuery(id.section(QLatin1Char('?'), 1));
    const auto color = query.queryItemValue(QStringLiteral("color"), QUrl::FullyDecoded);
    const auto stroke = query.queryItemValue(QStringLiteral("stroke"));
    QByteArray svg;
    {
        QMutexLocker lock(&mutex);
        auto it = rendered.constFind(renderKey);
        if (it != rendered.cend()) {
            if (size)
                *size = target;
            return it.value();
        }
        if (!sources.contains(name)) {
            QFile file(QStringLiteral(":/icons/%1.svg").arg(name));
            sources.insert(name, file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray());
        }
        svg = sources.value(name);
    }
    svg.replace("currentColor", (color.isEmpty() ? QStringLiteral("#ffffff") : color).toUtf8());
    if (!stroke.isEmpty())
        svg.replace("stroke-width=\"2\"", QStringLiteral("stroke-width=\"%1\"").arg(stroke).toUtf8());
    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    painter.end();
    {
        QMutexLocker lock(&mutex);
        if (rendered.size() > 512)
            rendered.clear();
        rendered.insert(renderKey, image);
    }
    if (size)
        *size = target;
    return image;
}

// ---------------------------------------------------------------- PanelImage

namespace {

// The page inside a scan's plain margin: the box around everything that
// differs from its corners, when they share a near-white or near-black
// colour. Pages without such a margin, or where trimming would take most
// of the page, stay whole.
QRect contentRect(const QImage &image) {
    const int w = image.width();
    const int h = image.height();
    if (w < 64 || h < 64)
        return image.rect();
    const QRgb corners[] = {image.pixel(1, 1), image.pixel(w - 2, 1), image.pixel(1, h - 2), image.pixel(w - 2, h - 2)};
    const auto close = [](QRgb a, QRgb b, int tolerance) {
        return std::abs(qRed(a) - qRed(b)) <= tolerance && std::abs(qGreen(a) - qGreen(b)) <= tolerance &&
               std::abs(qBlue(a) - qBlue(b)) <= tolerance;
    };
    const QRgb margin = corners[0];
    for (const auto corner : corners)
        if (!close(corner, margin, 24))
            return image.rect();
    const int level = qGray(margin);
    if (level > 30 && level < 225)
        return image.rect();
    // A line counts as content when more than a few of its pixels differ.
    const auto busyRow = [&](int y) {
        int count = 0;
        for (int x = 0; x < w; x += 2)
            if (!close(image.pixel(x, y), margin, 40) && ++count > w / 400 + 2)
                return true;
        return false;
    };
    const auto busyColumn = [&](int x, int top, int bottom) {
        int count = 0;
        for (int y = top; y <= bottom; y += 2)
            if (!close(image.pixel(x, y), margin, 40) && ++count > h / 400 + 2)
                return true;
        return false;
    };
    int top = 0;
    while (top < h - 1 && !busyRow(top))
        ++top;
    int bottom = h - 1;
    while (bottom > top && !busyRow(bottom))
        --bottom;
    int left = 0;
    while (left < w - 1 && !busyColumn(left, top, bottom))
        ++left;
    int right = w - 1;
    while (right > left && !busyColumn(right, top, bottom))
        --right;
    // A little air around the art.
    const int pad = std::max(2, w / 200);
    const QRect box = QRect(QPoint(left, top), QPoint(right, bottom)).adjusted(-pad, -pad, pad, pad) & image.rect();
    if (box.width() < w * 0.6 || box.height() < h * 0.6)
        return image.rect();
    return box;
}

} // namespace

PanelImage::PanelImage(QQuickItem *parent) : QQuickItem(parent) { setFlag(ItemHasContents, true); }

PanelImage::~PanelImage() {
    if (m_cancel)
        m_cancel->store(true);
}

// Pages bind source and referer to the same chapter result, and QML updates
// the two in no particular order. Starting on the source alone could request
// the page without its referer, which sites that protect their images answer
// with 403 (the first page in page mode stayed black), so both wait for the
// end of the update.
void PanelImage::setSource(const QString &source) {
    if (source == m_source)
        return;
    m_source = source;
    emit sourceChanged();
    if (isComponentComplete())
        startSoon();
}

void PanelImage::setReferer(const QString &referer) {
    if (referer == m_referer)
        return;
    m_referer = referer;
    emit sourceChanged();
    if (isComponentComplete() && !m_source.isEmpty() && m_status != Ready)
        startSoon();
}

void PanelImage::startSoon() {
    if (m_startQueued)
        return;
    m_startQueued = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            m_startQueued = false;
            start();
        },
        Qt::QueuedConnection);
}

void PanelImage::setMaxDecodeWidth(int width) {
    if (width == m_maxDecodeWidth)
        return;
    m_maxDecodeWidth = width;
    emit sourceChanged();
}

void PanelImage::setCropBorders(bool crop) {
    if (crop == m_cropBorders)
        return;
    m_cropBorders = crop;
    emit sourceChanged();
    startSoon();
}

void PanelImage::componentComplete() {
    QQuickItem::componentComplete();
    start();
}

void PanelImage::reload() { start(); }

void PanelImage::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        update();
}

void PanelImage::setStatus(Status status, const QString &error) {
    if (status == m_status && error == m_error)
        return;
    m_status = status;
    m_error = error;
    emit statusChanged();
}

void PanelImage::start() {
    if (m_cancel)
        m_cancel->store(true);
    m_pendingTiles.clear();
    m_tilesDirty = true;
    update();
    if (m_source.isEmpty()) {
        setStatus(Null);
        return;
    }
    setStatus(Loading);
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const int generation = ++m_generation;
    QPointer<PanelImage> self(this);
    panelPool().start([self, generation, cancel = m_cancel, source = m_source, referer = m_referer,
                       maxWidth = m_maxDecodeWidth, crop = m_cropBorders] {
        CancelScope scope(cancel);
        QList<QImage> tiles;
        QSize size;
        QString error;
        try {
            const auto data = ImageCache::fetch(source, referer);
            if (cancel->load())
                return;
            auto image = decode(data, {}, maxWidth);
            if (crop)
                image = image.copy(contentRect(image));
            size = image.size();
            image = image.convertToFormat(image.hasAlphaChannel() ? QImage::Format_RGBA8888_Premultiplied
                                                                  : QImage::Format_RGBX8888);
            for (int y = 0; y < image.height(); y += kTileHeight)
                tiles.append(image.copy(0, y, image.width(), std::min(kTileHeight, image.height() - y)));
        } catch (const sc::Error &e) {
            error = e.message();
        } catch (const std::exception &e) {
            error = QString::fromUtf8(e.what());
        }
        if (cancel->load())
            return;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, generation, tiles, size, error] {
                if (!self || self->m_generation != generation)
                    return;
                if (!error.isEmpty()) {
                    self->setStatus(Error, error);
                    return;
                }
                self->m_size = size;
                self->m_pendingTiles = tiles;
                self->m_tilesDirty = true;
                emit self->loaded();
                self->setStatus(Ready);
                self->update();
            },
            Qt::QueuedConnection);
    });
}

QSGNode *PanelImage::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
    auto *root = old ? old : new QSGNode;
    if (m_tilesDirty) {
        while (auto *child = root->firstChild()) {
            root->removeChildNode(child);
            delete child;
        }
        for (const auto &tile : std::as_const(m_pendingTiles)) {
            auto *node = window()->createImageNode();
            node->setTexture(window()->createTextureFromImage(tile));
            node->setOwnsTexture(true);
            node->setFiltering(QSGTexture::Linear);
            node->setMipmapFiltering(QSGTexture::None);
            root->appendChildNode(node);
        }
        // Textures now live on the GPU; drop the CPU copies.
        m_pendingTiles.clear();
        m_tilesDirty = false;
    }
    if (m_size.isEmpty() || !root->childCount())
        return root;
    const double scale = width() / m_size.width();
    double y = 0;
    for (auto *child = root->firstChild(); child; child = child->nextSibling()) {
        auto *node = static_cast<QSGImageNode *>(child);
        const auto texSize = node->texture()->textureSize();
        const double h = texSize.height() * (m_size.width() / double(texSize.width())) * scale;
        node->setRect(QRectF(0, y, width(), h));
        node->setSourceRect(QRectF(QPointF(0, 0), texSize));
        y += h;
    }
    return root;
}

} // namespace sc
