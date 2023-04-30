// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpengine/draw_context.h>
}

#include "libclient/drawdance/global.h"
#include <QLoggingCategory>

namespace drawdance {

#ifdef __GNUC__
#define PRINTF_LIKE [[gnu::format(printf, 5, 0)]]
#else
#define PRINTF_LIKE
#endif

PRINTF_LIKE static void logMessage(
	void *user, DP_LogLevel level, const char *file, int line, const char *fmt,
	va_list ap)
{
	const QLoggingCategory &category = *static_cast<QLoggingCategory *>(user);
	switch(level) {
	case DP_LOG_DEBUG:
		if(category.isDebugEnabled()) {
			qCDebug(
				category, "%s:%d - %s", file, line,
				qUtf8Printable(QString::vasprintf(fmt, ap)));
		}
		break;
	case DP_LOG_INFO:
		if(category.isInfoEnabled()) {
			qCInfo(
				category, "%s:%d - %s", file, line,
				qUtf8Printable(QString::vasprintf(fmt, ap)));
		}
		break;
	case DP_LOG_PANIC:
		if(category.isCriticalEnabled()) {
			qCCritical(
				category, "%s:%d - %s", file, line,
				qUtf8Printable(QString::vasprintf(fmt, ap)));
		}
		break;
	case DP_LOG_WARN:
	default:
		if(category.isWarningEnabled()) {
			qCWarning(
				category, "%s:%d - %s", file, line,
				qUtf8Printable(QString::vasprintf(fmt, ap)));
		}
		break;
	}
}

void initLogging()
{
	static QLoggingCategory category("drawdance");
	DP_log_fn_set(logMessage, &category);
}


void initCpuSupport()
{
    DP_cpu_support_init();
}


DrawContext::DrawContext(DrawContext &&other)
	: DrawContext{other.m_dc, other.m_pool}
{
	other.m_dc = nullptr;
	other.m_pool = nullptr;
}

DrawContext::~DrawContext()
{
	if(m_pool && m_dc) {
		m_pool->releaseContext(m_dc);
	}
}

DrawContext::DrawContext(DP_DrawContext *dc, DrawContextPool *pool)
	: m_dc{dc}
	, m_pool{pool}
{
}


namespace {
DrawContextPool *instance;
}

void DrawContextPool::init()
{
	Q_ASSERT(!instance);
	instance = new DrawContextPool;
}

void DrawContextPool::deinit()
{
	Q_ASSERT(instance);
	delete instance;
	instance = nullptr;
}

DrawContext DrawContextPool::acquire()
{
	Q_ASSERT(instance);
	return instance->acquireContext();
}

DrawContextPool::~DrawContextPool()
{
	for(DP_DrawContext *dc : m_dcs) {
		DP_draw_context_free(dc);
	}
}

DrawContext DrawContextPool::acquireContext()
{
	QMutexLocker locker{&m_mutex};
	DP_DrawContext *dc = m_dcs.isEmpty() ? DP_draw_context_new() : m_dcs.pop();
	return DrawContext{dc, this};
}

void DrawContextPool::releaseContext(DP_DrawContext *dc)
{
	Q_ASSERT(dc);
	QMutexLocker locker{&m_mutex};
	m_dcs.push(dc);
}

DrawContextPool::DrawContextPool()
	: m_mutex{}
	, m_dcs{}
{
}

}
