#include "CrashLog.h"

#include "core/Database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSysInfo>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <exception>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include <csignal>
#include <io.h>
#include <fcntl.h>
#else
#include <csignal>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <unwind.h>
#endif

namespace sc::crashlog {

namespace {

// The recent log: a ring written by every thread, read when crashing.
constexpr size_t kRing = 64 * 1024;
char g_ring[kRing];
std::atomic<size_t> g_written{0};
std::atomic_flag g_ringLock = ATOMIC_FLAG_INIT;

// Prepared at install, so a crash only writes: where the report goes, and
// its first lines.
char g_path[1024];
char g_header[2048];
// A fatal message or an uncaught exception's text, for the report.
char g_reason[2048];
std::atomic<int> g_crashing{0};
QtMessageHandler g_previous = nullptr;

constexpr int kKeep = 10;

QString folder() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/crashes"); }

void ringAppend(const char *data, size_t size) {
    while (g_ringLock.test_and_set(std::memory_order_acquire)) {
    }
    size_t at = g_written.load(std::memory_order_relaxed);
    for (size_t i = 0; i < size; ++i)
        g_ring[(at + i) % kRing] = data[i];
    g_written.store(at + size, std::memory_order_relaxed);
    g_ringLock.clear(std::memory_order_release);
}

// The ring in order, oldest first. While crashing, without waiting for a
// lock another (crashed) thread may hold.
template <typename Out>
void ringCopy(Out &&out, bool locking) {
    if (locking)
        while (g_ringLock.test_and_set(std::memory_order_acquire)) {
        }
    const size_t written = g_written.load(std::memory_order_relaxed);
    const size_t start = written > kRing ? written % kRing : 0;
    const size_t size = written > kRing ? kRing : written;
    if (start + size <= kRing) {
        out(g_ring + start, size);
    } else {
        out(g_ring + start, kRing - start);
        out(g_ring, size - (kRing - start));
    }
    if (locking)
        g_ringLock.clear(std::memory_order_release);
}

void onMessage(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    static const char *const levels[] = {"D", "W", "C", "F", "I"};
    QByteArray line = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")).toLatin1();
    line += ' ';
    line += levels[std::min<int>(type, 4)];
    line += ' ';
    if (context.category && std::strcmp(context.category, "default") != 0) {
        line += context.category;
        line += ": ";
    }
    line += message.toUtf8();
    if (type != QtDebugMsg && type != QtInfoMsg && context.file) {
        line += " (";
        line += QFileInfo(QString::fromUtf8(context.file)).fileName().toUtf8();
        line += ':';
        line += QByteArray::number(context.line);
        line += ')';
    }
    line += '\n';
    ringAppend(line.constData(), size_t(line.size()));
    if (type == QtFatalMsg)
        qstrncpy(g_reason, message.toUtf8().constData(), sizeof g_reason);
    if (g_previous)
        g_previous(type, context, message);
}

// ---------------------------------------------------------------- writing

#ifdef Q_OS_WIN
using Handle = HANDLE;
void put(Handle fd, const char *data, size_t size) {
    DWORD done = 0;
    WriteFile(fd, data, DWORD(size), &done, nullptr);
}
#else
using Handle = int;
void put(Handle fd, const char *data, size_t size) {
    while (size > 0) {
        const auto n = ::write(fd, data, size);
        if (n <= 0)
            return;
        data += n;
        size -= size_t(n);
    }
}
#endif

void put(Handle fd, const char *text) { put(fd, text, std::strlen(text)); }

void putHex(Handle fd, uintptr_t value) {
    char out[2 + 2 * sizeof value + 1];
    int at = int(sizeof out) - 1;
    out[at] = '\0';
    do {
        out[--at] = "0123456789abcdef"[value & 15];
        value >>= 4;
    } while (value);
    out[--at] = 'x';
    out[--at] = '0';
    put(fd, out + at);
}

void putNumber(Handle fd, long value) {
    char out[24];
    int at = int(sizeof out) - 1;
    out[at] = '\0';
    const bool negative = value < 0;
    unsigned long v = negative ? -value : value;
    do {
        out[--at] = char('0' + v % 10);
        v /= 10;
    } while (v);
    if (negative)
        out[--at] = '-';
    put(fd, out + at);
}

void putTail(Handle fd) {
    if (g_reason[0]) {
        put(fd, "Message: ");
        put(fd, g_reason);
        put(fd, "\n");
    }
}

void putLog(Handle fd) {
    put(fd, "\nRecent log:\n");
    ringCopy([fd](const char *data, size_t size) { put(fd, data, size); }, false);
}

#ifndef Q_OS_WIN
struct Walk {
    Handle fd;
    int frame;
};

_Unwind_Reason_Code onFrame(_Unwind_Context *context, void *argument) {
    auto *walk = static_cast<Walk *>(argument);
    const auto pc = uintptr_t(_Unwind_GetIP(context));
    if (!pc || walk->frame > 64)
        return _URC_END_OF_STACK;
    put(walk->fd, "#");
    putNumber(walk->fd, walk->frame++);
    put(walk->fd, " ");
    putHex(walk->fd, pc);
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(pc), &info) && info.dli_fname) {
        const char *name = std::strrchr(info.dli_fname, '/');
        put(walk->fd, " ");
        put(walk->fd, name ? name + 1 : info.dli_fname);
        put(walk->fd, "+");
        putHex(walk->fd, pc - uintptr_t(info.dli_fbase));
        if (info.dli_sname) {
            put(walk->fd, " ");
            put(walk->fd, info.dli_sname);
        }
    }
    put(walk->fd, "\n");
    return _URC_NO_REASON;
}

struct sigaction g_old[32];
const int kSignals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL};

const char *signalName(int signal) {
    switch (signal) {
    case SIGSEGV: return "SIGSEGV (invalid memory access)";
    case SIGABRT: return "SIGABRT (aborted)";
    case SIGBUS: return "SIGBUS (bus error)";
    case SIGFPE: return "SIGFPE (arithmetic error)";
    case SIGILL: return "SIGILL (illegal instruction)";
    }
    return "signal";
}

void onSignal(int signal, siginfo_t *info, void *context) {
    if (g_crashing.fetch_add(1) == 0) {
        const int fd = ::open(g_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            put(fd, g_header);
            put(fd, "What: ");
            put(fd, signalName(signal));
            if (info && (signal == SIGSEGV || signal == SIGBUS)) {
                put(fd, " at ");
                putHex(fd, uintptr_t(info->si_addr));
            }
            put(fd, "\n");
            putTail(fd);
            put(fd, "\nBacktrace:\n");
            Walk walk{fd, 0};
            _Unwind_Backtrace(onFrame, &walk);
            putLog(fd);
            ::close(fd);
        }
    }
    // The system's own handling (Android's crash dump, a core file).
    sigaction(signal, &g_old[signal], nullptr);
    if (g_old[signal].sa_flags & SA_SIGINFO) {
        if (g_old[signal].sa_sigaction)
            g_old[signal].sa_sigaction(signal, info, context);
    } else if (g_old[signal].sa_handler != SIG_DFL && g_old[signal].sa_handler != SIG_IGN) {
        g_old[signal].sa_handler(signal);
    }
    raise(signal);
}
#else
wchar_t g_widePath[1024];

void putStack(Handle fd) {
    void *frames[64];
    const USHORT count = RtlCaptureStackBackTrace(0, 64, frames, nullptr);
    for (USHORT i = 0; i < count; ++i) {
        const auto pc = uintptr_t(frames[i]);
        put(fd, "#");
        putNumber(fd, i);
        put(fd, " ");
        putHex(fd, pc);
        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(frames[i]), &module)) {
            char name[MAX_PATH];
            const DWORD n = GetModuleFileNameA(module, name, MAX_PATH);
            const char *base = name;
            for (DWORD k = 0; k < n; ++k)
                if (name[k] == '\\')
                    base = name + k + 1;
            put(fd, " ");
            put(fd, base);
            put(fd, "+");
            putHex(fd, pc - uintptr_t(module));
        }
        put(fd, "\n");
    }
}

void writeReport(const char *what, uintptr_t address) {
    if (g_crashing.fetch_add(1) != 0)
        return;
    HANDLE fd = CreateFileW(g_widePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd == INVALID_HANDLE_VALUE)
        return;
    put(fd, g_header);
    put(fd, "What: ");
    put(fd, what);
    if (address) {
        put(fd, " at ");
        putHex(fd, address);
    }
    put(fd, "\n");
    putTail(fd);
    put(fd, "\nBacktrace:\n");
    putStack(fd);
    putLog(fd);
    CloseHandle(fd);
}

LONG WINAPI onException(EXCEPTION_POINTERS *pointers) {
    char what[64] = "exception ";
    char code[16];
    DWORD value = pointers->ExceptionRecord->ExceptionCode;
    int at = 15;
    code[at] = '\0';
    do {
        code[--at] = "0123456789abcdef"[value & 15];
        value >>= 4;
    } while (value);
    std::strcat(what, "0x");
    std::strcat(what, code + at);
    writeReport(what, uintptr_t(pointers->ExceptionRecord->ExceptionAddress));
    return EXCEPTION_CONTINUE_SEARCH;
}

void onAbort(int) { writeReport("abort", 0); }
#endif

void onTerminate() {
    if (auto current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception &e) {
            qstrncpy(g_reason, (std::string("Uncaught exception: ") + e.what()).c_str(), sizeof g_reason);
        } catch (...) {
            qstrncpy(g_reason, "Uncaught exception of an unknown type", sizeof g_reason);
        }
    }
    std::abort();
}

QStringList seenList() { return Database::pref(QStringLiteral("crash.seen")).toStringList(); }

} // namespace

void install() {
    QDir().mkpath(folder());
    // The oldest go once there are more than kKeep.
    auto files = QDir(folder()).entryInfoList({QStringLiteral("crash-*.txt")}, QDir::Files, QDir::Time);
    for (int i = kKeep; i < files.size(); ++i)
        QFile::remove(files[i].absoluteFilePath());

    const auto stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const auto path = QDir::toNativeSeparators(folder() + QStringLiteral("/crash-%1-%2.txt").arg(stamp).arg(QCoreApplication::applicationPid()));
    qstrncpy(g_path, path.toLocal8Bit().constData(), sizeof g_path);
#ifdef Q_OS_WIN
    path.toWCharArray(g_widePath);
    g_widePath[std::min<qsizetype>(path.size(), 1023)] = 0;
#endif
    const auto header = QStringLiteral("SCANCORE crash report\nVersion: %1\nSystem: %2 (%3, %4)\nQt: %5\nStarted: %6\n")
                            .arg(QCoreApplication::applicationVersion(), QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(),
                                 QSysInfo::kernelVersion(), QString::fromLatin1(qVersion()),
                                 QDateTime::currentDateTime().toString(Qt::ISODate))
                            .toUtf8();
    qstrncpy(g_header, header.constData(), sizeof g_header);

    g_previous = qInstallMessageHandler(onMessage);
    std::set_terminate(onTerminate);
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(onException);
    std::signal(SIGABRT, onAbort);
#else
    // Room to report a stack overflow from.
    static char altStack[64 * 1024];
    stack_t stack{};
    stack.ss_sp = altStack;
    stack.ss_size = sizeof altStack;
    sigaltstack(&stack, nullptr);
    for (const int signal : kSignals) {
        struct sigaction action{};
        action.sa_sigaction = onSignal;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        sigaction(signal, &action, &g_old[signal]);
    }
#endif
}

QVariantList reports() {
    QVariantList out;
    const auto seen = seenList();
    for (const auto &file : QDir(folder()).entryInfoList({QStringLiteral("crash-*.txt")}, QDir::Files, QDir::Time)) {
        QFile f(file.absoluteFilePath());
        QString summary;
        if (f.open(QIODevice::ReadOnly))
            for (const auto &line : QString::fromUtf8(f.read(8192)).split(QLatin1Char('\n')))
                if (line.startsWith(QLatin1String("What: ")) || line.startsWith(QLatin1String("Message: ")))
                    summary += (summary.isEmpty() ? QString() : QStringLiteral(" · ")) + line.section(QLatin1String(": "), 1);
        out.append(QVariantMap{{"file", file.fileName()},
                               {"when", file.lastModified().toSecsSinceEpoch()},
                               {"summary", summary},
                               {"seen", seen.contains(file.fileName())}});
    }
    return out;
}

QString read(const QString &file) {
    if (file.contains(QLatin1Char('/')) || file.contains(QLatin1Char('\\')))
        return {};
    QFile f(folder() + QLatin1Char('/') + file);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

QStringList unseen() {
    QStringList out;
    for (const auto &value : reports())
        if (!value.toMap().value(QStringLiteral("seen")).toBool())
            out.append(value.toMap().value(QStringLiteral("file")).toString());
    return out;
}

void markSeen(const QString &file) {
    auto seen = seenList();
    if (!seen.contains(file)) {
        seen.append(file);
        while (seen.size() > 50)
            seen.removeFirst();
        Database::setPref(QStringLiteral("crash.seen"), seen);
    }
}

void remove(const QString &file) {
    if (!file.contains(QLatin1Char('/')) && !file.contains(QLatin1Char('\\')))
        QFile::remove(folder() + QLatin1Char('/') + file);
}

QString diagnostics() {
    QByteArray log;
    ringCopy([&log](const char *data, size_t size) { log.append(data, qsizetype(size)); }, true);
    return QString::fromUtf8(g_header).replace(QLatin1String("crash report"), QLatin1String("diagnostics")) +
           QStringLiteral("Now: %1\n\nRecent log:\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate)) + QString::fromUtf8(log);
}

} // namespace sc::crashlog
