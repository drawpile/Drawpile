// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWDANCE_GLOBAL_H
#define DRAWDANCE_GLOBAL_H
#include <QStack>
#include <QVector>

typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Mutex DP_Mutex;

namespace drawdance {


void initLogging();

void initCpuSupport();

void initImageImportExport();


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

struct DrawContextPoolStatistics {
    int contextsUsed;
    int contextsTotal;
    size_t bytesUsed;
    size_t bytesTotal;
};

class DrawContextPool final {
    friend DrawContext;
public:
    static void init();
    static void deinit();

    static DrawContext acquire();

    static DrawContextPoolStatistics statistics();

    ~DrawContextPool();

    DrawContextPool(const DrawContextPool &) = delete;
    DrawContextPool(DrawContextPool &&) = delete;
    DrawContextPool &operator=(const DrawContextPool &) = delete;
    DrawContextPool &operator=(DrawContextPool &&) = delete;

private:
    DrawContextPool();

    DrawContext acquireContext();
    void releaseContext(DP_DrawContext *dc);

    DrawContextPoolStatistics instanceStatistics();

    DP_Mutex *m_mutex;
    QStack<DP_DrawContext *> m_available;
    QVector<DP_DrawContext *> m_contexts;
};

}

#endif
