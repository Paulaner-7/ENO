#include "Layout.h"

#include <QHash>

#include <algorithm>
#include <numeric>

namespace sc::translation {

bool writesWithoutSpaces(const QString &language) {
    const auto base = language.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
    return base == QLatin1String("zh") || base == QLatin1String("ja") || base == QLatin1String("th");
}

namespace {

bool isVertical(const QRectF &box, const QString &text) { return box.height() > box.width() * 1.6 && text.size() > 1; }

double overlap(double a0, double a1, double b0, double b1) { return std::min(a1, b1) - std::max(a0, b0); }

int find(QList<int> &parent, int i) {
    while (parent[i] != i)
        i = parent[i] = parent[parent[i]];
    return i;
}

} // namespace

QList<TextBlock> groupLines(const QList<TextLine> &lines, const QString &language) {
    const int n = int(lines.size());
    QList<int> parent(n);
    std::iota(parent.begin(), parent.end(), 0);
    QList<bool> vertical(n);
    for (int i = 0; i < n; ++i)
        vertical[i] = isVertical(lines[i].box, lines[i].text);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (vertical[i] != vertical[j])
                continue;
            const auto &a = lines[i].box;
            const auto &b = lines[j].box;
            bool together = false;
            if (!vertical[i]) {
                // One under the other: a small gap, and sideways they share
                // part of their width (or their centres are close).
                const double size = std::min(a.height(), b.height());
                const double gap = std::max(a.top(), b.top()) - std::min(a.bottom(), b.bottom());
                const double side = overlap(a.left(), a.right(), b.left(), b.right());
                together = gap < size * 0.9 && (side > 0 || std::abs(a.center().x() - b.center().x()) < size * 1.5) &&
                           std::abs(a.height() - b.height()) < size * 0.8;
            } else {
                const double size = std::min(a.width(), b.width());
                const double gap = std::max(a.left(), b.left()) - std::min(a.right(), b.right());
                const double down = overlap(a.top(), a.bottom(), b.top(), b.bottom());
                together = gap < size * 0.9 && down > 0 && std::abs(a.width() - b.width()) < size * 0.8;
            }
            if (together)
                parent[find(parent, i)] = find(parent, j);
        }
    }

    QHash<int, QList<int>> groups;
    QList<int> order;
    for (int i = 0; i < n; ++i) {
        const int root = find(parent, i);
        if (!groups.contains(root))
            order.append(root);
        groups[root].append(i);
    }
    const bool tight = writesWithoutSpaces(language);
    QList<TextBlock> out;
    for (int root : std::as_const(order)) {
        auto members = groups.value(root);
        const bool columns = vertical[members.first()];
        std::sort(members.begin(), members.end(), [&](int x, int y) {
            if (columns)
                return lines[x].box.center().x() > lines[y].box.center().x();
            return lines[x].box.center().y() < lines[y].box.center().y();
        });
        TextBlock block;
        block.vertical = columns;
        QStringList parts;
        for (int m : std::as_const(members)) {
            block.box = block.box.isNull() ? lines[m].box : block.box.united(lines[m].box);
            parts.append(lines[m].text.trimmed());
        }
        block.text = parts.join(tight ? QString() : QStringLiteral(" "));
        // A word broken at a line's end ("won-" / "der") joins again.
        if (!tight)
            block.text.replace(QStringLiteral("- "), QString());
        if (!block.text.trimmed().isEmpty())
            out.append(block);
    }
    std::sort(out.begin(), out.end(), [](const TextBlock &a, const TextBlock &b) {
        // Rows of a page top to bottom; right to left within a row for
        // vertical writing, left to right otherwise.
        if (std::abs(a.box.top() - b.box.top()) > std::min(a.box.height(), b.box.height()) * 0.5)
            return a.box.top() < b.box.top();
        return a.vertical ? a.box.left() > b.box.left() : a.box.left() < b.box.left();
    });
    return out;
}

} // namespace sc::translation
