// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/common.h>
#include <dpcommon/cpu.h>
#include <dpcommon/threading.h>
#include <dpengine/draw_context.h>
#include <dpengine/tile.h>
#include <dpimpex/image_impex.h>
}
#include "libclient/drawdance/global.h"
#include <QLoggingCategory>

namespace drawdance {

#ifdef __GNUC__
#	define PRINTF_LIKE [[gnu::format(printf, 5, 0)]]
#else
#	define PRINTF_LIKE
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

void initImageImportExport()
{
	DP_image_impex_init();
}

void initStaticTiles()
{
	// These tiles are created on demand, so this shouldn't be necessary, but
	// we'll do it anyway just to be safe.
	DP_tile_censored_noinc();
	DP_tile_opaque_noinc();
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

DrawContextPoolStatistics DrawContextPool::statistics()
{
	Q_ASSERT(instance);
	return instance->instanceStatistics();
}

DrawContextPool::~DrawContextPool()
{
	for(DP_DrawContext *dc : m_available) {
		DP_draw_context_free(dc);
	}
	DP_mutex_free(m_mutex);
}

DrawContext DrawContextPool::acquireContext()
{
	DP_DrawContext *dc;
	DP_MUTEX_MUST_LOCK(m_mutex);
	if(m_available.isEmpty()) {
		dc = DP_draw_context_new();
		m_contexts.append(dc);
	} else {
		dc = m_available.pop();
	}
	DP_MUTEX_MUST_UNLOCK(m_mutex);
	return DrawContext{dc, this};
}

void DrawContextPool::releaseContext(DP_DrawContext *dc)
{
	Q_ASSERT(dc);
	DP_MUTEX_MUST_LOCK(m_mutex);
	m_available.push(dc);
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

DrawContextPoolStatistics DrawContextPool::instanceStatistics()
{
	DP_MUTEX_MUST_LOCK(m_mutex);
	int contextsTotal = m_contexts.size();
	int contextsUsed = contextsTotal - m_available.size();

	size_t bytesTotal = 0;
	for(DP_DrawContext *dc : m_contexts) {
		DP_DrawContextStatistics dcs = DP_draw_context_statistics(dc);
		bytesTotal += dcs.static_bytes + dcs.pool_bytes;
	}

	size_t bytesAvailable = 0;
	for(DP_DrawContext *dc : m_available) {
		DP_DrawContextStatistics dcs = DP_draw_context_statistics(dc);
		bytesAvailable += dcs.static_bytes + dcs.pool_bytes;
	}

	DP_MUTEX_MUST_UNLOCK(m_mutex);
	return DrawContextPoolStatistics{
		contextsUsed, contextsTotal, bytesTotal - bytesAvailable, bytesTotal};
}

DrawContextPool::DrawContextPool()
	: m_mutex(DP_mutex_new())
{
}

}
