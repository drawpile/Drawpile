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
#include "layer_props.h"
#include "layer_props_list.h"
#include "pixels.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>
#include <dpmsg/blend_mode.h>


typedef struct DP_LayerTitle {
    DP_Atomic refcount;
    size_t length;
    char title[];
} DP_LayerTitle;

#ifdef DP_NO_STRICT_ALIASING

struct DP_LayerProps {
    DP_Atomic refcount;
    const bool transient;
    const int id;
    const uint16_t opacity;
    int blend_mode;
    const bool hidden;
    const bool censored;
    const bool isolated;
    DP_LayerTitle *const title;
    struct {
        DP_LayerPropsList *const children;
    };
};

struct DP_TransientLayerProps {
    DP_Atomic refcount;
    bool transient;
    int id;
    uint16_t opacity;
    int blend_mode;
    bool hidden;
    bool censored;
    bool isolated;
    DP_LayerTitle *title;
    union {
        DP_LayerPropsList *children;
        DP_TransientLayerPropsList *transient_children;
    };
};

#else

struct DP_LayerProps {
    DP_Atomic refcount;
    bool transient;
    int id;
    uint16_t opacity;
    int blend_mode;
    bool hidden;
    bool censored;
    bool isolated;
    DP_LayerTitle *title;
    union {
        DP_LayerPropsList *children;
        DP_TransientLayerPropsList *transient_children;
    };
};

#endif


static DP_LayerTitle *layer_title_new(const char *title, size_t length)
{
    DP_ASSERT(length < SIZE_MAX);
    DP_LayerTitle *lt = DP_malloc(sizeof(*lt) + length + 1);
    DP_atomic_set(&lt->refcount, 1);
    if (length > 0) {
        DP_ASSERT(title);
        memcpy(lt->title, title, length);
    }
    lt->title[length] = '\0';
    lt->length = length;
    return lt;
}

static DP_LayerTitle *layer_title_incref(DP_LayerTitle *lt)
{
    DP_ASSERT(lt);
    DP_ASSERT(DP_atomic_get(&lt->refcount) > 0);
    DP_atomic_inc(&lt->refcount);
    return lt;
}

static DP_LayerTitle *layer_title_incref_nullable(DP_LayerTitle *lt)
{
    return lt ? layer_title_incref(lt) : NULL;
}

static void layer_title_decref(DP_LayerTitle *lt)
{
    DP_ASSERT(lt);
    DP_ASSERT(DP_atomic_get(&lt->refcount) > 0);
    if (DP_atomic_dec(&lt->refcount)) {
        DP_free(lt);
    }
}

static void layer_title_decref_nullable(DP_LayerTitle *lt)
{
    if (lt) {
        layer_title_decref(lt);
    }
}


DP_LayerProps *DP_layer_props_incref(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    DP_atomic_inc(&lp->refcount);
    return lp;
}

DP_LayerProps *DP_layer_props_incref_nullable(DP_LayerProps *lp_or_null)
{
    return lp_or_null ? DP_layer_props_incref(lp_or_null) : NULL;
}

void DP_layer_props_decref(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    if (DP_atomic_dec(&lp->refcount)) {
        DP_layer_props_list_decref_nullable(lp->children);
        layer_title_decref_nullable(lp->title);
        DP_free(lp);
    }
}

void DP_layer_props_decref_nullable(DP_LayerProps *lp_or_null)
{
    if (lp_or_null) {
        DP_layer_props_decref(lp_or_null);
    }
}

int DP_layer_props_refcount(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return DP_atomic_get(&lp->refcount);
}

bool DP_layer_props_transient(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->transient;
}

int DP_layer_props_id(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->id;
}

uint16_t DP_layer_props_opacity(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->opacity;
}

int DP_layer_props_blend_mode(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->blend_mode;
}

bool DP_layer_props_hidden(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->hidden;
}

bool DP_layer_props_censored(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->censored;
}

bool DP_layer_props_isolated(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->isolated;
}

bool DP_layer_props_visible(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->opacity > 0 && !lp->hidden;
}

const char *DP_layer_props_title(DP_LayerProps *lp, size_t *out_length)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);

    DP_LayerTitle *lt = lp->title;
    const char *title;
    size_t length;
    if (lt) {
        DP_ASSERT(DP_atomic_get(&lt->refcount) > 0);
        title = lt->title;
        length = lt->length;
    }
    else {
        title = "";
        length = 0;
    }

    if (out_length) {
        *out_length = length;
    }
    return title;
}

DP_LayerPropsList *DP_layer_props_children_noinc(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    return lp->children;
}

bool DP_layer_props_differ(DP_LayerProps *lp, DP_LayerProps *prev_lp)
{
    return lp != prev_lp
        && (lp->opacity != prev_lp->opacity
            || lp->blend_mode != prev_lp->blend_mode
            || lp->hidden != prev_lp->hidden
            || lp->censored != prev_lp->censored
            || lp->isolated != prev_lp->isolated);
}


static DP_TransientLayerProps *alloc_transient_layer_props(DP_LayerProps *lp)
{
    DP_TransientLayerProps *tlp = DP_malloc(sizeof(*tlp));
    *tlp = (DP_TransientLayerProps){
        DP_ATOMIC_INIT(1),
        true,
        lp->id,
        lp->opacity,
        lp->blend_mode,
        lp->hidden,
        lp->censored,
        lp->isolated,
        layer_title_incref_nullable(lp->title),
        {NULL},
    };
    return tlp;
}

DP_TransientLayerProps *DP_transient_layer_props_new(DP_LayerProps *lp)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    DP_ASSERT(!lp->transient);
    DP_ASSERT(!lp->children || !DP_layer_props_list_transient(lp->children));
    DP_TransientLayerProps *tlp = alloc_transient_layer_props(lp);
    tlp->children = DP_layer_props_list_incref_nullable(lp->children);
    return tlp;
}

DP_TransientLayerProps *DP_transient_layer_props_new_with_children_noinc(
    DP_LayerProps *lp, DP_TransientLayerPropsList *tlpl)
{
    DP_ASSERT(lp);
    DP_ASSERT(DP_atomic_get(&lp->refcount) > 0);
    DP_ASSERT(!lp->transient);
    DP_ASSERT(tlpl);
    DP_TransientLayerProps *tlp = alloc_transient_layer_props(lp);
    tlp->transient_children = tlpl;
    return tlp;
}

DP_TransientLayerProps *DP_transient_layer_props_new_init(int layer_id,
                                                          bool group)
{
    DP_TransientLayerProps *tlp = DP_malloc(sizeof(*tlp));
    *tlp = (DP_TransientLayerProps){
        DP_ATOMIC_INIT(1),
        true,
        layer_id,
        DP_BIT15,
        DP_BLEND_MODE_NORMAL,
        false,
        false,
        group,
        NULL,
        {group ? DP_layer_props_list_new() : NULL},
    };
    return tlp;
}

DP_TransientLayerProps *
DP_transient_layer_props_incref(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return (DP_TransientLayerProps *)DP_layer_props_incref(
        (DP_LayerProps *)tlp);
}

void DP_transient_layer_props_decref(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    DP_layer_props_decref((DP_LayerProps *)tlp);
}

int DP_transient_layer_props_refcount(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_refcount((DP_LayerProps *)tlp);
}

DP_LayerProps *DP_transient_layer_props_persist(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    DP_LayerPropsList *children = tlp->children;
    if (children && DP_layer_props_list_transient(children)) {
        DP_transient_layer_props_list_persist(tlp->transient_children);
    }
    tlp->transient = false;
    return (DP_LayerProps *)tlp;
}

int DP_transient_layer_props_id(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_id((DP_LayerProps *)tlp);
}

uint16_t DP_transient_layer_props_opacity(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_opacity((DP_LayerProps *)tlp);
}

int DP_transient_layer_props_blend_mode(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_blend_mode((DP_LayerProps *)tlp);
}

bool DP_transient_layer_props_hidden(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_hidden((DP_LayerProps *)tlp);
}

bool DP_transient_layer_props_censored(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_censored((DP_LayerProps *)tlp);
}

bool DP_transient_layer_props_isolated(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_isolated((DP_LayerProps *)tlp);
}

bool DP_transient_layer_props_visible(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_visible((DP_LayerProps *)tlp);
}

const char *DP_transient_layer_props_title(DP_TransientLayerProps *tlp,
                                           size_t *out_length)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_title((DP_LayerProps *)tlp, out_length);
}

DP_LayerPropsList *
DP_transient_layer_props_children_noinc(DP_TransientLayerProps *tlp)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    return DP_layer_props_children_noinc((DP_LayerProps *)tlp);
}

DP_TransientLayerPropsList *
DP_transient_layer_props_transient_children(DP_TransientLayerProps *tlp,
                                            int reserve)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    DP_ASSERT(reserve >= 0);
    DP_LayerPropsList *lpl = tlp->children;
    if (!DP_layer_props_list_transient(lpl)) {
        tlp->transient_children =
            DP_transient_layer_props_list_new(lpl, reserve);
        DP_layer_props_list_decref(lpl);
    }
    else if (reserve > 0) {
        tlp->transient_children = DP_transient_layer_props_list_reserve(
            tlp->transient_children, reserve);
    }
    return tlp->transient_children;
}

void DP_transient_layer_props_id_set(DP_TransientLayerProps *tlp, int layer_id)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    tlp->id = layer_id;
}

void DP_transient_layer_props_opacity_set(DP_TransientLayerProps *tlp,
                                          uint16_t opacity)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    tlp->opacity = opacity;
}

void DP_transient_layer_props_blend_mode_set(DP_TransientLayerProps *tlp,
                                             int blend_mode)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    tlp->blend_mode = blend_mode;
}

void DP_transient_layer_props_censored_set(DP_TransientLayerProps *tlp,
                                           bool censored)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    tlp->censored = censored;
}

void DP_transient_layer_props_hidden_set(DP_TransientLayerProps *tlp,
                                         bool hidden)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    tlp->hidden = hidden;
}

void DP_transient_layer_props_isolated_set(DP_TransientLayerProps *tlp,
                                           bool isolated)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    tlp->isolated = isolated;
}

void DP_transient_layer_props_title_set(DP_TransientLayerProps *tlp,
                                        const char *title, size_t length)
{
    DP_ASSERT(tlp);
    DP_ASSERT(DP_atomic_get(&tlp->refcount) > 0);
    DP_ASSERT(tlp->transient);
    layer_title_decref_nullable(tlp->title);
    tlp->title = layer_title_new(title, length);
}
