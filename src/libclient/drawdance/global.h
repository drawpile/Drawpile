#ifndef DRAWDANCE_GLOBAL_H
#define DRAWDANCE_GLOBAL_H

#include <QMutex>
#include <QStack>

typedef struct DP_DrawContext DP_DrawContext;

namespace drawdance {


void initLogging();

void initCpuSupport();


class DrawContextPool;

class DrawContext final {
    friend DrawContextPool;
public:
    DrawContext(DrawContext &&);
    ~DrawContext();

    DrawContext(const DrawContext &) = delete;
    DrawContext &operator=(const DrawContext &) = delete;
    DrawContext &operator=(DrawContext &&) = delete;

    DP_DrawContext *get() { return m_dc; }

private:
    DrawContext(DP_DrawContext *dc, DrawContextPool *pool);

    DP_DrawContext *m_dc;
    DrawContextPool *m_pool;
};

class DrawContextPool final {
    friend DrawContext;
public:
    static void init();
    static void deinit();

    static DrawContext acquire();

    ~DrawContextPool();

    DrawContextPool(const DrawContextPool &) = delete;
    DrawContextPool(DrawContextPool &&) = delete;
    DrawContextPool &operator=(const DrawContextPool &) = delete;
    DrawContextPool &operator=(DrawContextPool &&) = delete;

private:
    DrawContextPool();

    DrawContext acquireContext();
    void releaseContext(DP_DrawContext *dc);

    QMutex m_mutex;
    QStack<DP_DrawContext *> m_dcs;
};

}

#endif
