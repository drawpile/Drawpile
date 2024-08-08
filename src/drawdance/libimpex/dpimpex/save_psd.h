// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_SAVE_PSD_H
#define DPIMPEX_SAVE_PSD_H
#include "save.h"
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;

DP_SaveResult DP_save_psd(DP_CanvasState *cs, const char *path,
                          DP_DrawContext *dc);

#endif
