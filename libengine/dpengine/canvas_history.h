/*
 * Copyright (C) 2022 askmeaboutloom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * This code is based on Drawpile, using it under the GNU General Public
 * License, version 3. See 3rdparty/licenses/drawpile/COPYING for details.
 */
#ifndef DPENGINE_CANVAS_HISTORY_H
#define DPENGINE_CANVAS_HISTORY_H
#include <dpcommon/common.h>

typedef struct DP_CanvasState DP_CanvasState;
typedef struct DP_DrawContext DP_DrawContext;
typedef struct DP_Message DP_Message;


typedef struct DP_CanvasHistory DP_CanvasHistory;

DP_CanvasHistory *DP_canvas_history_new(void);

void DP_canvas_history_free(DP_CanvasHistory *ch);

void DP_canvas_history_local_pen_down_set(DP_CanvasHistory *ch,
                                          bool local_pen_down);

DP_CanvasState *DP_canvas_history_compare_and_get(DP_CanvasHistory *ch,
                                                  DP_CanvasState *prev);

void DP_canvas_history_reset(DP_CanvasHistory *ch);

void DP_canvas_history_soft_reset(DP_CanvasHistory *ch);

bool DP_canvas_history_handle(DP_CanvasHistory *ch, DP_DrawContext *dc,
                              DP_Message *msg);

bool DP_canvas_history_handle_local(DP_CanvasHistory *ch, DP_DrawContext *dc,
                                    DP_Message *msg);


#endif
