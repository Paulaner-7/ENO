#pragma once

#include <QByteArray>

namespace sc {

// Server-sent events: blocks separated by a blank line, "field: value"
// lines. Takes the whole blocks out of `buffer` (keeping a partial one) and
// calls `handle(event, data)` for each one with data.
template <typename Handle>
void takeEvents(QByteArray &buffer, Handle handle) {
    buffer.replace("\r\n", "\n");
    qsizetype end;
    while ((end = buffer.indexOf("\n\n")) >= 0) {
        const auto block = buffer.left(end);
        buffer.remove(0, end + 2);
        QByteArray event = "message", data;
        for (const auto &line : block.split('\n')) {
            if (line.isEmpty() || line.startsWith(':'))
                continue;
            const auto colon = line.indexOf(':');
            const auto field = colon < 0 ? line : line.left(colon);
            auto value = colon < 0 ? QByteArray() : line.mid(colon + 1);
            if (value.startsWith(' '))
                value.remove(0, 1);
            if (field == "event")
                event = value;
            else if (field == "data")
                data += (data.isEmpty() ? QByteArray() : QByteArray("\n")) + value;
        }
        if (!data.isEmpty())
            handle(event, data);
    }
}

} // namespace sc
