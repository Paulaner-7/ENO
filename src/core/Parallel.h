#pragma once

#include "Http.h"

#include <QDeadlineTimer>
#include <QList>
#include <QMutex>
#include <QThreadPool>
#include <QWaitCondition>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>
#include <memory>
#include <optional>

namespace sc {

QThreadPool &fanoutPool();

template <typename T>
struct Outcome {
    std::optional<T> value;
    QString error;
    int status = 0;
    QString verifyUrl;
    bool ok() const { return value.has_value(); }
};

// Runs jobs concurrently on the fan-out pool and waits for all of them. The
// caller's cancel flag is carried into every job, so cancelling a search
// stops all of its source requests.
template <typename T>
QList<Outcome<T>> parallel(const QList<std::function<T()>> &jobs) {
    const auto cancel = CancelScope::current();
    QList<QFuture<Outcome<T>>> futures;
    futures.reserve(jobs.size());
    for (const auto &job : jobs) {
        futures.append(QtConcurrent::run(&fanoutPool(), [job, cancel]() -> Outcome<T> {
            CancelScope scope(cancel);
            Outcome<T> outcome;
            try {
                outcome.value = job();
            } catch (const Error &e) {
                outcome.error = e.message();
                outcome.status = e.status();
                outcome.verifyUrl = e.verifyUrl();
            } catch (const std::exception &e) {
                outcome.error = QString::fromUtf8(e.what());
            }
            return outcome;
        }));
    }
    QList<Outcome<T>> results;
    results.reserve(futures.size());
    for (auto &future : futures)
        results.append(future.result());
    return results;
}

// Like parallel(), but hands each outcome to `each(index, outcome)` on the
// calling thread as soon as it is ready, fastest first. Jobs still running
// after `timeoutMs` are cancelled and reported as timed out (status 504), so
// one slow source cannot hold the rest back.
template <typename T>
void parallelEach(const QList<std::function<T()>> &jobs, int timeoutMs,
                  const std::function<void(int, const Outcome<T> &)> &each) {
    struct Shared {
        QMutex mutex;
        QWaitCondition ready;
        QList<QPair<int, Outcome<T>>> done;
    };
    const auto shared = std::make_shared<Shared>();
    const auto parent = CancelScope::current();
    QList<CancelFlag> flags;
    flags.reserve(jobs.size());
    for (int i = 0; i < jobs.size(); ++i) {
        const auto flag = std::make_shared<std::atomic_bool>(false);
        flags.append(flag);
        fanoutPool().start([job = jobs[i], flag, shared, i] {
            CancelScope scope(flag);
            Outcome<T> outcome;
            try {
                outcome.value = job();
            } catch (const Error &e) {
                outcome.error = e.message();
                outcome.status = e.status();
                outcome.verifyUrl = e.verifyUrl();
            } catch (const std::exception &e) {
                outcome.error = QString::fromUtf8(e.what());
            }
            QMutexLocker lock(&shared->mutex);
            shared->done.append({i, std::move(outcome)});
            shared->ready.wakeAll();
        });
    }
    QList<bool> reported(jobs.size(), false);
    int remaining = int(jobs.size());
    const QDeadlineTimer deadline(timeoutMs);
    while (remaining > 0) {
        QList<QPair<int, Outcome<T>>> batch;
        {
            QMutexLocker lock(&shared->mutex);
            if (shared->done.isEmpty())
                shared->ready.wait(&shared->mutex, QDeadlineTimer(std::min<qint64>(100, deadline.remainingTime())));
            batch.swap(shared->done);
        }
        for (const auto &[index, outcome] : batch) {
            reported[index] = true;
            --remaining;
            each(index, outcome);
        }
        const bool cancelled = parent && parent->load();
        if (remaining > 0 && (cancelled || deadline.hasExpired())) {
            for (int i = 0; i < jobs.size(); ++i) {
                if (reported[i])
                    continue;
                flags[i]->store(true);
                if (cancelled)
                    continue;
                Outcome<T> late;
                late.error = QStringLiteral("It took too long to answer.");
                late.status = 504;
                each(i, late);
            }
            if (cancelled)
                throw Error(QStringLiteral("Cancelled"), 499);
            return;
        }
    }
}

} // namespace sc
