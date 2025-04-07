// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphic.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpcommon/geom.h>


typedef struct DP_GraphicElement {
    DP_Atomic refcount;
    int x;
    int y;
    size_t size;
    unsigned char data[];
} DP_GraphicElement;

#ifdef DP_NO_STRICT_ALIASING

struct DP_Graphic {
    DP_Atomic refcount;
    const bool transient;
    const DP_Quad quad;
    const int width;
    const int height;
    const int count;
    const DP_GraphicElement *elements[];
};

struct DP_TransientGraphic {
    DP_Atomic refcount;
    bool transient;
    DP_Quad quad;
    int width;
    int height;
    int count;
    DP_GraphicElement *elements[];
};

#else

struct DP_TransientGraphic {
    DP_Atomic refcount;
    bool transient;
    DP_Quad quad;
    int width;
    int height;
    int count;
    DP_GraphicElement *elements[];
};

#endif


DP_Graphic *DP_graphic_incref(DP_Graphic *g)
{
    DP_ASSERT(g);
    DP_ASSERT(DP_atomic_get(&g->refcount) > 0);
    DP_atomic_inc(&g->refcount);
    return g;
}

DP_Graphic *DP_graphic_incref_nullable(DP_Graphic *g_or_null)
{
    return g_or_null ? DP_graphic_incref(g_or_null) : NULL;
}

void DP_graphic_decref(DP_Graphic *g)
{
    DP_ASSERT(g);
    DP_ASSERT(DP_atomic_get(&g->refcount) > 0);
    if (DP_atomic_dec(&g->refcount)) {
        DP_free(g);
    }
}

void DP_graphic_decref_nullable(DP_Graphic *g_or_null)
{
    if (g_or_null) {
        DP_graphic_decref(g_or_null);
    }
}

int DP_graphic_refcount(DP_Graphic *g)
{
    DP_ASSERT(g);
    DP_ASSERT(DP_atomic_get(&g->refcount) > 0);
    return DP_atomic_get(&g->refcount);
}

bool DP_graphic_transient(DP_Graphic *g)
{
    DP_ASSERT(g);
    DP_ASSERT(DP_atomic_get(&g->refcount) > 0);
    return g->transient;
}


DP_TransientGraphic *DP_transient_graphic_incref(DP_TransientGraphic *tg)
{
    return (DP_TransientGraphic *)DP_graphic_incref((DP_Graphic *)tg);
}

DP_TransientGraphic *
DP_transient_graphic_incref_nullable(DP_TransientGraphic *tg_or_null)
{
    return (DP_TransientGraphic *)DP_graphic_incref((DP_Graphic *)tg_or_null);
}

void DP_transient_graphic_decref(DP_TransientGraphic *tg)
{
    DP_graphic_decref((DP_Graphic *)tg);
}

void DP_transient_graphic_decref_nullable(DP_TransientGraphic *tg)
{
    DP_graphic_decref_nullable((DP_Graphic *)tg);
}

int DP_transient_graphic_refcount(DP_TransientGraphic *tg)
{
    return DP_graphic_refcount((DP_Graphic *)tg);
}

DP_Graphic *DP_transient_graphic_persist(DP_TransientGraphic *tg)
{
    DP_ASSERT(tg);
    DP_ASSERT(DP_atomic_get(&tg->refcount) > 0);
    DP_ASSERT(tg->transient);
    tg->transient = false;
    return (DP_Graphic *)tg;
}
