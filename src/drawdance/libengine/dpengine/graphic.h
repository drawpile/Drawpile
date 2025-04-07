// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_GRAPHIC_H
#define DPENGINE_GRAPHIC_H
#include <dpcommon/common.h>

#define DP_GRAPHIC_LAYER_BACKGROUND -1
#define DP_GRAPHIC_LAYER_FOREGROUND 0

typedef struct DP_Graphic DP_Graphic;

#ifdef DP_NO_STRICT_ALIASING
typedef struct DP_TransientGraphic DP_TransientGraphic;
#else
typedef struct DP_Graphic DP_TransientGraphic;
#endif


DP_Graphic *DP_graphic_incref(DP_Graphic *g);

DP_Graphic *DP_graphic_incref_nullable(DP_Graphic *g_or_null);

void DP_graphic_decref(DP_Graphic *g);

void DP_graphic_decref_nullable(DP_Graphic *g_or_null);

int DP_graphic_refcount(DP_Graphic *g);

bool DP_graphic_transient(DP_Graphic *g);


DP_TransientGraphic *DP_transient_graphic_incref(DP_TransientGraphic *tg);

DP_TransientGraphic *
DP_transient_graphic_incref_nullable(DP_TransientGraphic *tg_or_null);

void DP_transient_graphic_decref(DP_TransientGraphic *tg);

void DP_transient_graphic_decref_nullable(DP_TransientGraphic *tg_or_null);

int DP_transient_graphic_refcount(DP_TransientGraphic *tg);

DP_Graphic *DP_transient_graphic_persist(DP_TransientGraphic *tg);


#endif
