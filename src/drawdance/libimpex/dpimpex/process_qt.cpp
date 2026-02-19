// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "process.h"
}
#include <QProcess>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using StringListSizeType = qsizetype;
#else
using StringListSizeType = int;
#endif

// This backend shouldn't be used on Android or the browser, process_dummy.c
// should get compiled for those instead.
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#    error "Bad platform for process backend"
#endif

extern "C" bool DP_process_supported(void)
{
    return true;
}

extern "C" DP_Process *DP_process_new(int argc, const char **argv)
{
    if (argc < 1 || !argv || !argv[0]) {
        DP_error_set("Invalid process arguments");
        return nullptr;
    }

    QProcess *process = new QProcess;
    process->setProgram(QString::fromUtf8(argv[0]));

    QStringList arguments;
    arguments.reserve(StringListSizeType(argc - 1));
    for (int i = 1; i < argc; ++i) {
        arguments.append(QString::fromUtf8(argv[i]));
    }
    process->setArguments(arguments);

    process->setInputChannelMode(QProcess::ManagedInputChannel);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    process->start(QIODevice::WriteOnly);
    if (!process->waitForStarted(10000)) {
        process->kill();
        delete process;
        DP_error_set("Process did not start");
        return nullptr;
    }

    return reinterpret_cast<DP_Process *>(process);
}

extern "C" int DP_process_free_close_wait(DP_Process *p)
{
    int code = -1;
    if (p) {
        QProcess *process = reinterpret_cast<QProcess *>(p);
        process->closeWriteChannel();
        process->waitForFinished(-1);
        if (process->exitStatus() == QProcess::NormalExit) {
            code = process->exitCode();
        }
        delete process;
    }
    return code;
}

extern "C" void DP_process_kill(DP_Process *p)
{
    DP_ASSERT(p);
    QProcess *process = reinterpret_cast<QProcess *>(p);
    process->kill();
}

extern "C" long long DP_process_pid(DP_Process *p)
{
    DP_ASSERT(p);
    QProcess *process = reinterpret_cast<QProcess *>(p);
    return static_cast<long long>(process->processId());
}

extern "C" bool DP_process_running(DP_Process *p)
{
    DP_ASSERT(p);
    QProcess *process = reinterpret_cast<QProcess *>(p);
    return process->state() == QProcess::Running;
}

extern "C" bool DP_process_write(DP_Process *p, size_t len,
                                 const unsigned char *data)
{
    if (len > 0 && data) {
        DP_ASSERT(p);
        QProcess *process = reinterpret_cast<QProcess *>(p);
        qint64 written =
            process->write(reinterpret_cast<const char *>(data), qint64(len));
        if (written < qint64(0)) {
            DP_error_set("Error writing to process: %s",
                         qUtf8Printable(process->errorString()));
            return false;
        }
        else if (size_t(written) != len) {
            DP_error_set("Tried to write %zu byte(s) to process, but wrote %zu",
                         len, size_t(written));
            return false;
        }
        else if (!process->waitForBytesWritten(-1)) {
            DP_error_set("Error during process write: %s",
                         qUtf8Printable(process->errorString()));
            return false;
        }
    }
    return true;
}

extern "C" void DP_process_close(DP_Process *p)
{
    DP_ASSERT(p);
    QProcess *process = reinterpret_cast<QProcess *>(p);
    process->closeWriteChannel();
}

extern "C" bool DP_process_wait(DP_Process *p, int msec)
{
    DP_ASSERT(p);
    QProcess *process = reinterpret_cast<QProcess *>(p);
    return process->waitForFinished(msec);
}
