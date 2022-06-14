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
#ifndef DP_ENGINE_MODEL_CHANGES
#define DP_ENGINE_MODEL_CHANGES
#include <dpcommon/common.h>

typedef struct DP_Annotation DP_Annotation;
typedef struct DP_AnnotationList DP_AnnotationList;


typedef int (*DP_ModelChangesGetCountFn)(void *model);
typedef void *(*DP_ModelChangesGetElementFn)(void *model, int i);
typedef int (*DP_ModelChangesGetIndexByIdFn)(void *model, int id);
typedef int (*DP_ModelChangesGetIdFn)(void *element);
typedef bool (*DP_ModelChangesIsDifferentFn)(void *DP_RESTRICT prev_element,
                                             void *DP_RESTRICT next_element);
typedef void (*DP_ModelChangesAddedFn)(int id, int index, void *element,
                                       void *user);
typedef void (*DP_ModelChangesRemovedFn)(int id, void *user);
typedef void (*DP_ModelChangesMovedFn)(int id, int index, void *user);
typedef void (*DP_ModelChangesChangedFn)(int id, void *element, void *user);

typedef struct DP_ModelChangesMethods {
    DP_ModelChangesGetCountFn get_count;
    DP_ModelChangesGetElementFn get_element;
    DP_ModelChangesGetIndexByIdFn get_index_by_id;
    DP_ModelChangesGetIdFn get_id;
    DP_ModelChangesIsDifferentFn is_different;
    DP_ModelChangesAddedFn added;
    DP_ModelChangesRemovedFn removed;
    DP_ModelChangesMovedFn moved;
    DP_ModelChangesChangedFn changed;
} DP_ModelChangesMethods;


typedef struct DP_ModelChanges DP_ModelChanges;

DP_ModelChanges *DP_model_changes_new(void);

void DP_model_changes_free(DP_ModelChanges *mc);


void DP_model_changes_diff(DP_ModelChanges *mc, void *prev_model_or_null,
                           void *next_model_or_null,
                           const DP_ModelChangesMethods *methods, void *user);


typedef void (*DP_ModelChangesAnnotationAddedFn)(int annotation_id, int index,
                                                 DP_Annotation *a, void *user);

typedef void (*DP_ModelChangesAnnotationRemovedFn)(int annotation_id,
                                                   void *user);

typedef void (*DP_ModelChangesAnnotationMovedFn)(int annotation_id, int index,
                                                 void *user);

typedef void (*DP_ModelChangesAnnotationChangedFn)(int annotation_id,
                                                   DP_Annotation *a,
                                                   void *user);

void DP_model_changes_diff_annotations(
    DP_ModelChanges *mc, DP_AnnotationList *prev_or_null,
    DP_AnnotationList *next_or_null, DP_ModelChangesAnnotationAddedFn added,
    DP_ModelChangesAnnotationRemovedFn removed,
    DP_ModelChangesAnnotationMovedFn moved,
    DP_ModelChangesAnnotationChangedFn changed, void *user);


#endif
