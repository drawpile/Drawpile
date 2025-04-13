// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_SELECTION_H
#define DPENGINE_SELECTION_H
#include <dpcommon/common.h>

typedef struct DP_Rect DP_Rect;
typedef struct DP_CanvasDiff DP_CanvasDiff;
DP_TYPEDEF_PERSISTENT(LayerContent);
DP_TYPEDEF_PERSISTENT(Selection);


DP_Selection *DP_selection_new_init(unsigned int context_id, int selection_id,
                                    DP_LayerContent *lc, const DP_Rect *bounds);

DP_Selection *DP_selection_incref(DP_Selection *sel);

DP_Selection *DP_selection_incref_nullable(DP_Selection *sel_or_null);

void DP_selection_decref(DP_Selection *sel);

void DP_selection_decref_nullable(DP_Selection *sel_or_null);

int DP_selection_refcount(DP_Selection *sel);

bool DP_selection_transient(DP_Selection *sel);

unsigned int DP_selection_context_id(DP_Selection *sel);

int DP_selection_id(DP_Selection *sel);

const DP_Rect *DP_selection_bounds(DP_Selection *sel);

DP_LayerContent *DP_selection_content_noinc(DP_Selection *sel);

// May return NULL if the resulting selection would be empty.
DP_Selection *DP_selection_resize(DP_Selection *sel, int top, int right,
                                  int bottom, int left);

void DP_selection_diff_nullable(DP_Selection *sel_or_null,
                                DP_Selection *prev_or_null,
                                DP_CanvasDiff *diff);


DP_TransientSelection *DP_transient_selection_new(DP_Selection *sel);

DP_TransientSelection *DP_transient_selection_new_init(unsigned int context_id,
                                                       int selection_id,
                                                       int width, int height);

DP_TransientSelection *
DP_transient_selection_incref(DP_TransientSelection *tsel);

void DP_transient_selection_decref(DP_TransientSelection *tsel);

int DP_transient_selection_refcount(DP_TransientSelection *tsel);

// May decrement return NULL if the selection is empty. The owner should then.
// decrement the refcount and get rid of its reference to this selection.
DP_Selection *DP_transient_selection_persist(DP_TransientSelection *tsel);

unsigned int DP_transient_selection_context_id(DP_TransientSelection *tsel);

int DP_transient_selection_id(DP_TransientSelection *tsel);

const DP_Rect *DP_transient_selection_bounds(DP_TransientSelection *tsel);

DP_LayerContent *
DP_transient_selection_content_noinc(DP_TransientSelection *tsel);

DP_TransientLayerContent *
DP_transient_selection_transient_content_noinc(DP_TransientSelection *tsel);


#endif
