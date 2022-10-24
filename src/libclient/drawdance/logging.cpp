extern "C" {
#include <dpcommon/common.h>
}
#include "logging.h"
#include <QLoggingCategory>

namespace drawdance {

namespace {

void logMessage(void *user, DP_LogLevel level, const char *file, int line, const char *fmt, va_list ap)
{
    const QLoggingCategory &category = *static_cast<QLoggingCategory *>(user);
    switch (level) {
    case DP_LOG_DEBUG:
        if (category.isDebugEnabled()) {
            qCDebug(category, "%s:%d - %s", file, line, qUtf8Printable(QString::vasprintf(fmt, ap)));
        }
        break;
    case DP_LOG_PANIC:
        if (category.isCriticalEnabled()) {
            qCCritical(category, "%s:%d - %s", file, line, qUtf8Printable(QString::vasprintf(fmt, ap)));
        }
        break;
    case DP_LOG_WARN:
    default:
        if (category.isWarningEnabled()) {
            qCWarning(category, "%s:%d - %s", file, line, qUtf8Printable(QString::vasprintf(fmt, ap)));
        }
        break;
    }
}

}

void initLogging()
{
    static QLoggingCategory category("drawdance");
    DP_log_fn_set(logMessage, &category);
}

}
