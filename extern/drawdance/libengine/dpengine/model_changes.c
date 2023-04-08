/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "model_changes.h"
#include "annotation.h"
#include "annotation_list.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>


typedef struct DP_ModelChangesEntry {
    int id;
    void *element;
} DP_ModelChangesEntry;

struct DP_ModelChanges {
    int capacity;
    DP_ModelChangesEntry *entries;
};

DP_ModelChanges *DP_model_changes_new(void)
{
    DP_ModelChanges *mc = DP_malloc(sizeof(*mc));
    (*mc) = (DP_ModelChanges){0, NULL};
    return mc;
}

void DP_model_changes_free(DP_ModelChanges *mc)
{
    if (mc) {
        DP_free(mc->entries);
        DP_free(mc);
    }
}


static void add_all(void *next_model, int next_count,
                    const DP_ModelChangesMethods *methods, void *user)
{
    DP_ModelChangesGetElementFn get_element = methods->get_element;
    DP_ModelChangesGetIdFn get_id = methods->get_id;
    DP_ModelChangesAddedFn added = methods->added;
    for (int i = 0; i < next_count; ++i) {
        void *element = get_element(next_model, i);
        int id = get_id(element);
        added(id, i, element, user);
    }
}

static void remove_all(void *prev_model, int prev_count,
                       const DP_ModelChangesMethods *methods, void *user)
{
    DP_ModelChangesGetElementFn get_element = methods->get_element;
    DP_ModelChangesGetIdFn get_id = methods->get_id;
    DP_ModelChangesRemovedFn removed = methods->removed;
    for (int i = 0; i < prev_count; ++i) {
        void *element = get_element(prev_model, i);
        int id = get_id(element);
        removed(id, user);
    }
}


static int entries_index_by_id(DP_ModelChangesEntry *entries, int count, int id)
{
    for (int i = 0; i < count; ++i) {
        if (entries[i].id == id) {
            return i;
        }
    }
    return -1;
}

static void entries_remove(DP_ModelChangesEntry *entries, int count, int i)
{
    memmove(entries + i, entries + i + 1,
            sizeof(*entries) * DP_int_to_size(count - i - 1));
}

static void entries_add(DP_ModelChangesEntry *entries, int count, int i, int id,
                        void *element)
{
    memmove(entries + i + 1, entries + i,
            sizeof(*entries) * DP_int_to_size(count - i));
    entries[i] = (DP_ModelChangesEntry){id, element};
}


static int diff_models_removals(int prev_count, DP_ModelChangesEntry *entries,
                                void *next_model,
                                DP_ModelChangesGetIndexByIdFn get_index_by_id,
                                DP_ModelChangesRemovedFn removed, void *user)
{
    for (int i = 0; i < prev_count;) {
        int id = entries[i].id;
        if (get_index_by_id(next_model, id) == -1) {
            removed(id, user);
            entries_remove(entries, prev_count, i);
            --prev_count;
        }
        else {
            ++i;
        }
    }
    return prev_count;
}

static int diff_models_additions(int prev_count, int next_count,
                                 DP_ModelChangesEntry *entries,
                                 void *next_model,
                                 DP_ModelChangesGetElementFn get_element,
                                 DP_ModelChangesGetIdFn get_id,
                                 DP_ModelChangesAddedFn added, void *user)
{
    for (int i = 0; i < next_count; ++i) {
        void *element = get_element(next_model, i);
        int id = get_id(element);
        if (entries_index_by_id(entries, prev_count, id) == -1) {
            added(id, i, element, user);
            entries_add(entries, prev_count, i, id, element);
            ++prev_count;
        }
    }
    return prev_count;
}

// Moving stuff is a bit complicated, since moving an element shifts the
// position of all following elements too. So the algorithm always moves indexes
// from smallest to largest, meaning that there can never be an element placed
// before one that's already in place, so their positions don't get shifted.

static bool search_next_move(int count, DP_ModelChangesEntry *entries,
                             void *next_model, int start,
                             DP_ModelChangesGetElementFn get_element,
                             DP_ModelChangesGetIdFn get_id,
                             int *out_found_prev_index,
                             int *out_found_next_index)
{
    bool found = false;
    int found_prev_index, found_next_index;

    for (int next_index = start; next_index < count; ++next_index) {
        void *element = get_element(next_model, next_index);
        int id = get_id(element);
        int prev_index = entries_index_by_id(entries, count, id);
        DP_ASSERT(prev_index >= 0);
        DP_ASSERT(prev_index < count);
        if (prev_index != next_index
            && (!found
                || DP_min_int(prev_index, next_index)
                       < DP_min_int(found_prev_index, found_next_index))) {
            found = true;
            found_prev_index = prev_index;
            found_next_index = next_index;
        }
    }

    if (found) {
        *out_found_prev_index = found_prev_index;
        *out_found_next_index = found_next_index;
        return true;
    }
    else {
        return false;
    }
}

static void diff_models_moves(int count, DP_ModelChangesEntry *entries,
                              void *next_model,
                              DP_ModelChangesGetElementFn get_element,
                              DP_ModelChangesGetIdFn get_id,
                              DP_ModelChangesMovedFn moved, void *user)
{
    int start = 0;
    int prev_index, next_index;
    while (search_next_move(count, entries, next_model, start, get_element,
                            get_id, &prev_index, &next_index)) {
        DP_ModelChangesEntry prev_entry = entries[prev_index];
        moved(prev_entry.id, next_index, user);
        entries_remove(entries, count, prev_index);
        entries_add(entries, count - 1,
                    prev_index < next_index ? next_index - 1 : next_index,
                    prev_entry.id, prev_entry.element);
        start = DP_min_int(prev_index, next_index) + 1;
    }
}

static void diff_models_changes(int count, DP_ModelChangesEntry *entries,
                                void *next_model,
                                DP_ModelChangesGetElementFn get_element,
                                DP_ModelChangesIsDifferentFn is_different,
                                DP_ModelChangesChangedFn changed, void *user)
{
    for (int i = 0; i < count; ++i) {
        DP_ModelChangesEntry prev_entry = entries[i];
        void *next_element = get_element(next_model, i);
        if (prev_entry.element != next_element
            && is_different(prev_entry.element, next_element)) {
            changed(prev_entry.id, next_element, user);
        }
    }
}

static void diff_models(int prev_count, int next_count,
                        DP_ModelChangesEntry *entries, void *next_model,
                        const DP_ModelChangesMethods *methods, void *user)
{
    DP_ModelChangesGetElementFn get_element = methods->get_element;
    DP_ModelChangesGetIndexByIdFn get_index_by_id = methods->get_index_by_id;
    DP_ModelChangesGetIdFn get_id = methods->get_id;
    int count = prev_count;

    count = diff_models_removals(count, entries, next_model, get_index_by_id,
                                 methods->removed, user);

    count = diff_models_additions(count, next_count, entries, next_model,
                                  get_element, get_id, methods->added, user);

    DP_ASSERT(count == next_count);
    diff_models_moves(count, entries, next_model, get_element, get_id,
                      methods->moved, user);

    diff_models_changes(count, entries, next_model, get_element,
                        methods->is_different, methods->changed, user);
}


static void prepare_diff_models(DP_ModelChanges *mc,
                                void *DP_RESTRICT prev_model,
                                void *DP_RESTRICT next_model, int prev_count,
                                int next_count,
                                const DP_ModelChangesMethods *methods,
                                void *user)
{
    int required_capacity = DP_max_int(prev_count, next_count);
    if (mc->capacity < required_capacity) {
        size_t size = sizeof(*mc->entries) * DP_int_to_size(required_capacity);
        mc->entries = DP_realloc(mc->entries, size);
    }

    DP_ModelChangesEntry *entries = mc->entries;
    DP_ModelChangesGetElementFn get_element = methods->get_element;
    DP_ModelChangesGetIdFn get_id = methods->get_id;
    for (int i = 0; i < prev_count; ++i) {
        void *element = get_element(prev_model, i);
        entries[i] = (DP_ModelChangesEntry){get_id(element), element};
    }

    diff_models(prev_count, next_count, entries, next_model, methods, user);
}

static void check_empty_models(DP_ModelChanges *mc,
                               void *DP_RESTRICT prev_model,
                               void *DP_RESTRICT next_model,
                               const DP_ModelChangesMethods *methods,
                               void *user)
{
    DP_ModelChangesGetCountFn get_count = methods->get_count;
    int prev_count = get_count(prev_model);
    DP_ASSERT(prev_count >= 0);
    int next_count = get_count(next_model);
    DP_ASSERT(next_count >= 0);
    if (prev_count != 0) {
        if (next_count != 0) {
            prepare_diff_models(mc, prev_model, next_model, prev_count,
                                next_count, methods, user);
        }
        else {
            remove_all(prev_model, prev_count, methods, user);
        }
    }
    else if (next_count != 0) {
        add_all(next_model, next_count, methods, user);
    }
}

void DP_model_changes_diff(DP_ModelChanges *mc, void *prev_model_or_null,
                           void *next_model_or_null,
                           const DP_ModelChangesMethods *methods, void *user)
{
    DP_ASSERT(mc);
    DP_ASSERT(methods);
    DP_ASSERT(methods->get_count);
    DP_ASSERT(methods->get_element);
    DP_ASSERT(methods->get_index_by_id);
    DP_ASSERT(methods->get_id);
    DP_ASSERT(methods->is_different);
    DP_ASSERT(methods->added);
    DP_ASSERT(methods->removed);
    DP_ASSERT(methods->moved);
    DP_ASSERT(methods->changed);
    if (prev_model_or_null != next_model_or_null) {
        if (prev_model_or_null) {
            if (next_model_or_null) {
                check_empty_models(mc, prev_model_or_null, next_model_or_null,
                                   methods, user);
            }
            else {
                int prev_count = methods->get_count(prev_model_or_null);
                remove_all(prev_model_or_null, prev_count, methods, user);
            }
        }
        else if (next_model_or_null) {
            int next_count = methods->get_count(next_model_or_null);
            add_all(next_model_or_null, next_count, methods, user);
        }
    }
}


typedef struct DP_ModelChangesAnnotationArgs {
    DP_ModelChangesAnnotationAddedFn added;
    DP_ModelChangesAnnotationRemovedFn removed;
    DP_ModelChangesAnnotationMovedFn moved;
    DP_ModelChangesAnnotationChangedFn changed;
    void *user;
} DP_ModelChangesAnnotationArgs;

static int annotation_list_get_count(void *model)
{
    return DP_annotation_list_count(model);
}

static void *annotation_list_get_element(void *model, int i)
{
    return DP_annotation_list_at_noinc(model, i);
}

static int annotation_list_get_index_by_id(void *model, int id)
{
    return DP_annotation_list_index_by_id(model, id);
}

static int annotation_get_id(void *element)
{
    return DP_annotation_id(element);
}

static bool annotation_is_different(void *DP_RESTRICT prev_element,
                                    void *DP_RESTRICT next_element)
{
    return !DP_annotation_equal(prev_element, next_element);
}

static void annotation_added(int id, int index, void *element, void *user)
{
    DP_ModelChangesAnnotationArgs *args = user;
    DP_ModelChangesAnnotationAddedFn added = args->added;
    if (added) {
        added(id, index, element, args->user);
    }
}

static void annotation_removed(int id, void *user)
{
    DP_ModelChangesAnnotationArgs *args = user;
    DP_ModelChangesAnnotationRemovedFn removed = args->removed;
    if (removed) {
        removed(id, args->user);
    }
}

static void annotation_moved(int id, int index, void *user)
{
    DP_ModelChangesAnnotationArgs *args = user;
    DP_ModelChangesAnnotationMovedFn moved = args->moved;
    if (moved) {
        moved(id, index, args->user);
    }
}

static void annotation_changed(int id, void *element, void *user)
{
    DP_ModelChangesAnnotationArgs *args = user;
    DP_ModelChangesAnnotationChangedFn changed = args->changed;
    if (changed) {
        changed(id, element, args->user);
    }
}

static const DP_ModelChangesMethods annotations_methods = {
    annotation_list_get_count,
    annotation_list_get_element,
    annotation_list_get_index_by_id,
    annotation_get_id,
    annotation_is_different,
    annotation_added,
    annotation_removed,
    annotation_moved,
    annotation_changed,
};

void DP_model_changes_diff_annotations(
    DP_ModelChanges *mc, DP_AnnotationList *prev_or_null,
    DP_AnnotationList *next_or_null, DP_ModelChangesAnnotationAddedFn added,
    DP_ModelChangesAnnotationRemovedFn removed,
    DP_ModelChangesAnnotationMovedFn moved,
    DP_ModelChangesAnnotationChangedFn changed, void *user)
{
    DP_ModelChangesAnnotationArgs args = {added, removed, moved, changed, user};
    DP_model_changes_diff(mc, prev_or_null, next_or_null, &annotations_methods,
                          &args);
}
