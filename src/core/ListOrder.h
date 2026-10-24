#pragma once

#include <QStringList>

#include <cstdlib>

namespace sc::lists {

// Every list id in the order shown: `stored` first (ids that no longer
// exist dropped), then any list it does not name yet (new ones, or from an
// older version) after the list before it in `all`, the default order.
inline QStringList ordered(const QStringList &all, const QStringList &stored) {
    QStringList out;
    for (const auto &id : stored)
        if (all.contains(id) && !out.contains(id))
            out.append(id);
    for (int i = 0; i < all.size(); ++i)
        if (!out.contains(all[i]))
            out.insert(i == 0 ? 0 : out.indexOf(all[i - 1]) + 1, all[i]);
    return out;
}

// `order` with `id` moved `by` places; with skipHidden, lists in `hidden`
// are passed over (they are not shown where it moves). Unchanged when it
// cannot move that far.
inline QStringList moved(QStringList order, const QStringList &hidden, const QString &id, int by, bool skipHidden) {
    const int at = order.indexOf(id);
    if (at < 0 || by == 0)
        return order;
    const int step = by > 0 ? 1 : -1;
    int to = at;
    for (int left = std::abs(by); left > 0;) {
        to += step;
        if (to < 0 || to >= order.size())
            return order;
        if (!(skipHidden && hidden.contains(order[to])))
            --left;
    }
    order.move(at, to);
    return order;
}

} // namespace sc::lists
