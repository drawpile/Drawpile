extern "C" {
#include <dpengine/draw_context.h>
}

#include "drawcontextpool.h"

namespace drawdance {


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
{}



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
