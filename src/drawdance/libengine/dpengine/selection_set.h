// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPENGINE_SELECTION_SET_H
#define DPENGINE_SELECTION_SET_H
#include <dpcommon/common.h>

typedef struct DP_CanvasDiff DP_CanvasDiff;
DP_TYPEDEF_PERSISTENT(SelectionSet);
DP_TYPEDEF_PERSISTENT(Selection);


DP_SelectionSet *DP_selection_set_incref(DP_SelectionSet *ss);

DP_SelectionSet *DP_selection_set_incref_nullable(DP_SelectionSet *ss_or_null);

void DP_selection_set_decref(DP_SelectionSet *ss);

void DP_selection_set_decref_nullable(DP_SelectionSet *ss_or_null);

int DP_selection_set_refcount(DP_SelectionSet *ss);

bool DP_selection_set_transient(DP_SelectionSet *ss);

int DP_selection_set_count(DP_SelectionSet *ss);

DP_Selection *DP_selection_set_search_noinc(DP_SelectionSet *ss,
                                            unsigned int context_id,
                                            int selection_id);

int DP_selection_set_search_index(DP_SelectionSet *ss, unsigned int context_id,
                                  int selection_id);

DP_Selection *DP_selection_set_at_noinc(DP_SelectionSet *ss, int index);

// May return NULL if all selections end up empty after the resize.
DP_TransientSelectionSet *DP_selection_set_resize(DP_SelectionSet *ss, int top,
                                                  int right, int bottom,
                                                  int left);

// Removes selections matching the given context id and selection id. If the
// selection id is zero, only the context id is matched. Returns a new transient
// selection set if anything was removed and NULL otherwise.
DP_TransientSelectionSet *DP_selection_set_remove(DP_SelectionSet *ss,
                                                  unsigned int context_id,
                                                  int selection_id);

void DP_selection_set_diff_nullable(DP_SelectionSet *ss_or_null,
                                    unsigned int active_context_id,
                                    int active_selection_id,
                                    DP_SelectionSet *prev_or_null,
                                    unsigned int prev_active_context_id,
                                    int prev_active_selection_id,
                                    DP_CanvasDiff *diff);


DP_TransientSelectionSet *DP_transient_selection_set_new_init(int reserve);

DP_TransientSelectionSet *DP_transient_selection_set_new(DP_SelectionSet *ss,
                                                         int reserve);

DP_TransientSelectionSet *
DP_transient_selection_set_incref(DP_TransientSelectionSet *tss);

DP_TransientSelectionSet *DP_transient_selection_set_incref_nullable(
    DP_TransientSelectionSet *tss_or_null);

void DP_transient_selection_set_decref(DP_TransientSelectionSet *tss);

void DP_transient_selection_set_decref_nullable(
    DP_TransientSelectionSet *tss_or_null);

int DP_transient_selection_set_refcount(DP_TransientSelectionSet *tss);

// May return NULL if all selections are empty. The owner should then decrement
// the refcount and get rid of the reference to this selection set.
DP_SelectionSet *
DP_transient_selection_set_persist(DP_TransientSelectionSet *tss);

int DP_transient_selection_set_count(DP_TransientSelectionSet *tss);

DP_TransientSelection *
DP_transient_selection_set_transient_at_noinc(DP_TransientSelectionSet *tss,
                                              int index);

void DP_transient_selection_set_insert_at_noinc(DP_TransientSelectionSet *tss,
                                                int index, DP_Selection *sel);

void DP_transient_selection_set_insert_transient_at_noinc(
    DP_TransientSelectionSet *tss, int index, DP_TransientSelection *tsel);

void DP_transient_selection_set_replace_at_noinc(DP_TransientSelectionSet *tss,
                                                 int index, DP_Selection *sel);

void DP_transient_selection_set_delete_at(DP_TransientSelectionSet *tss,
                                          int index);

#endif
