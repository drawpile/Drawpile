// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_SELECTION_H
#define DPENGINE_SELECTION_H
#include <dpcommon/common.h>

typedef struct DP_Rect DP_Rect;

DP_TYPEDEF_PERSISTENT(LayerContent);

// We currently don't need a transient version of this.
typedef struct DP_Selection DP_Selection;

DP_Selection *DP_selection_new_init(unsigned int context_id, int selection_id,
                                    DP_LayerContent *lc, const DP_Rect *bounds);

DP_Selection *DP_selection_incref(DP_Selection *sel);

DP_Selection *DP_selection_incref_nullable(DP_Selection *sel_or_null);

void DP_selection_decref(DP_Selection *sel);

void DP_selection_decref_nullable(DP_Selection *sel_or_null);

int DP_selection_refcount(DP_Selection *sel);

unsigned int DP_selection_context_id(DP_Selection *sel);

int DP_selection_id(DP_Selection *sel);

const DP_Rect *DP_selection_bounds(DP_Selection *sel);

DP_LayerContent *DP_selection_content_noinc(DP_Selection *sel);

// May return NULL if the resulting selection would be empty.
DP_Selection *DP_selection_resize(DP_Selection *sel, int top, int right,
                                  int bottom, int left);


#endif
